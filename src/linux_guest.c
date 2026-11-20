/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "libos.h"
#include "linux_elf32.h"

#define STACK_BASE (LINUX_GUEST_STACK_TOP - LINUX_GUEST_STACK_SIZE)
#define MAX_VECTOR_COUNT 8u
#define MAX_STRING_SIZE 256u

enum { AT_NULL = 0, AT_PHDR = 3, AT_PHENT = 4, AT_PHNUM = 5, AT_PAGESZ = 6, AT_ENTRY = 9 };

struct initial_stack {
    size_t args[MAX_VECTOR_COUNT], env[MAX_VECTOR_COUNT];
    uint32_t strings, esp;
};

static enum linux_guest_status measure_strings(size_t count, const char *const vector[],
                                               size_t lengths[], uint32_t *total)
{
    if (count > MAX_VECTOR_COUNT) return LINUX_GUEST_STACK;
    if (count && !vector) return LINUX_GUEST_ARGUMENT;
    for (size_t i = 0; i < count; ++i) {
        if (!vector[i]) return LINUX_GUEST_ARGUMENT;
        size_t size = 0;
        while (size < MAX_STRING_SIZE && vector[i][size]) ++size;
        if (size == MAX_STRING_SIZE) return LINUX_GUEST_STACK;
        lengths[i] = size + 1;
        *total += (uint32_t)(size + 1);
    }
    return LINUX_GUEST_OK;
}

static enum linux_guest_status plan_stack(struct initial_stack *stack, int has_phdr,
                                         size_t argc, const char *const argv[],
                                         size_t envc, const char *const envp[])
{
    if (!argc) return LINUX_GUEST_ARGUMENT;
    uint32_t strings = 0;
    enum linux_guest_status status = measure_strings(argc,argv,stack->args,&strings);
    if (status != LINUX_GUEST_OK) return status;
    status = measure_strings(envc,envp,stack->env,&strings);
    if (status != LINUX_GUEST_OK) return status;

    unsigned aux_words = has_phdr ? 12u : 6u;
    uint32_t table = 4u * (uint32_t)(argc + envc + 3u + aux_words);
    stack->strings = LINUX_GUEST_STACK_TOP - strings;
    stack->esp = (stack->strings - table) & ~UINT32_C(15);
    return stack->esp >= STACK_BASE ? LINUX_GUEST_OK : LINUX_GUEST_STACK;
}

static void put_word(unsigned char **dest, uint32_t value)
{
    for (unsigned i = 0; i < 4; ++i) {
        *(*dest)++ = (unsigned char)value;
        value >>= 8;
    }
}

static void put_vector(struct linux_guest *guest, unsigned char **table, uint32_t *start,
                       size_t count, const char *const vector[], const size_t lengths[])
{
    for (size_t i = 0; i < count; ++i) {
        put_word(table,*start);
        memcpy(guest->stack + (*start - STACK_BASE),vector[i],lengths[i]);
        *start += (uint32_t)lengths[i];
    }
    put_word(table,0);
}

static void build_stack(struct linux_guest *guest, const struct initial_stack *stack,
                        const struct linux_elf_layout *elf, size_t argc, const char *const argv[],
                        size_t envc, const char *const envp[])
{
    unsigned char *table = guest->stack + (stack->esp - STACK_BASE);
    uint32_t strings = stack->strings;
    put_word(&table,(uint32_t)argc);
    put_vector(guest,&table,&strings,argc,argv,stack->args);
    put_vector(guest,&table,&strings,envc,envp,stack->env);
    if (elf->phdr) {
        put_word(&table,AT_PHDR);
        put_word(&table,elf->phdr);
        put_word(&table,AT_PHENT);
        put_word(&table,ELF32_PHENT);
        put_word(&table,AT_PHNUM);
        put_word(&table,elf->phnum);
    }
    put_word(&table,AT_PAGESZ);
    put_word(&table,LINUX_GUEST_PAGE_SIZE);
    put_word(&table,AT_ENTRY);
    put_word(&table,elf->entry);
    put_word(&table,AT_NULL);
    put_word(&table,0);
}

enum linux_guest_status linux_guest_load_elf(struct linux_guest *guest,
                                            const void *file, size_t size,
                                            size_t argc, const char *const argv[],
                                            size_t envc, const char *const envp[])
{
    if (!guest || !file) return LINUX_GUEST_ARGUMENT;
    struct linux_elf_layout elf;
    enum linux_guest_status status = linux_elf32_parse(&elf,file,size);
    if (status != LINUX_GUEST_OK) return status;
    struct initial_stack stack;
    status = plan_stack(&stack,elf.phdr != 0,argc,argv,envc,envp);
    if (status != LINUX_GUEST_OK) return status;

    /* All fallible validation happens before replacing the live image. */
    memset(guest,0,sizeof(*guest));
    for (unsigned i = 0; i < elf.count; ++i) {
        const struct linux_elf_segment *segment = &elf.segments[i];
        guest->regions[i] = segment->region;
        memcpy(guest->image + segment->region.offset,
               (const unsigned char *)file + segment->file_offset,segment->file_size);
    }
    build_stack(guest,&stack,&elf,argc,argv,envc,envp);
    guest->cpu = (struct linux_i386_cpu){ .eip = elf.entry, .esp = stack.esp, .eflags = 0x202u };
    guest->region_count = elf.count;
    return LINUX_GUEST_OK;
}

/* A non-empty request must fit entirely in one region. Return a byte offset
   in a native backing array, never a pointer derived from a guest address. */
static enum linux_guest_status translate(const struct linux_guest *guest, uint32_t address,
                                         size_t size, uint32_t flags, uint32_t *offset, int *stack)
{
    if (!guest || !size) return LINUX_GUEST_ARGUMENT;
    if (!guest->region_count) return LINUX_GUEST_NOT_LOADED;
    if (address >= STACK_BASE && address < LINUX_GUEST_STACK_TOP) {
        if (size > LINUX_GUEST_STACK_TOP - address) return LINUX_GUEST_UNMAPPED;
        if (flags == LINUX_PF_X) return LINUX_GUEST_PERMISSION;
        *offset = address - STACK_BASE;
        *stack = 1;
        return LINUX_GUEST_OK;
    }
    for (unsigned i = 0; i < guest->region_count; ++i) {
        const struct linux_guest_region *region = &guest->regions[i];
        if (address < region->base || address - region->base >= region->size) continue;
        uint32_t step = address - region->base;
        if (size > region->size - step) return LINUX_GUEST_UNMAPPED;
        if (!(region->flags & flags)) return LINUX_GUEST_PERMISSION;
        *offset = region->offset + step;
        *stack = 0;
        return LINUX_GUEST_OK;
    }
    return LINUX_GUEST_UNMAPPED;
}

static enum linux_guest_status copy_out(const struct linux_guest *guest, uint32_t address,
                                       void *buffer, size_t size, uint32_t flags)
{
    if (!buffer) return LINUX_GUEST_ARGUMENT;
    uint32_t offset;
    int stack;
    enum linux_guest_status status = translate(guest,address,size,flags,&offset,&stack);
    if (status == LINUX_GUEST_OK) memcpy(buffer,(stack ? guest->stack : guest->image) + offset,size);
    return status;
}

enum linux_guest_status linux_guest_read(const struct linux_guest *guest, uint32_t address,
                                        void *buffer, size_t size)
{
    return copy_out(guest,address,buffer,size,LINUX_PF_R);
}

enum linux_guest_status linux_guest_write(struct linux_guest *guest, uint32_t address,
                                         const void *buffer, size_t size)
{
    if (!buffer) return LINUX_GUEST_ARGUMENT;
    uint32_t offset;
    int stack;
    enum linux_guest_status status = translate(guest,address,size,LINUX_PF_W,&offset,&stack);
    if (status == LINUX_GUEST_OK) memcpy((stack ? guest->stack : guest->image) + offset,buffer,size);
    return status;
}

enum linux_guest_status linux_guest_fetch(const struct linux_guest *guest, uint32_t address,
                                         void *buffer, size_t size)
{
    return copy_out(guest,address,buffer,size,LINUX_PF_X);
}

enum linux_guest_status linux_guest_step(const struct linux_guest *guest, uint8_t *opcode)
{
    if (!guest) return LINUX_GUEST_ARGUMENT;
    enum linux_guest_status status = linux_guest_fetch(guest,guest->cpu.eip,opcode,1);
    return status == LINUX_GUEST_OK ? LINUX_GUEST_UNSUPPORTED_INSTRUCTION : status;
}

const char *linux_guest_error(enum linux_guest_status status)
{
    static const char *const errors[] = {
        [LINUX_GUEST_OK] = "ok",
        [LINUX_GUEST_ARGUMENT] = "invalid argument",
        [LINUX_GUEST_FORMAT] = "invalid ELF header",
        [LINUX_GUEST_UNSUPPORTED_ELF] = "unsupported ELF feature",
        [LINUX_GUEST_TRUNCATED] = "ELF range outside file",
        [LINUX_GUEST_SEGMENT] = "invalid LOAD segment",
        [LINUX_GUEST_ALIGNMENT] = "invalid LOAD alignment",
        [LINUX_GUEST_OVERLAP] = "overlapping LOAD pages",
        [LINUX_GUEST_ENTRY] = "entry outside executable file bytes",
        [LINUX_GUEST_CAPACITY] = "ELF capacity exceeded",
        [LINUX_GUEST_STACK] = "initial stack capacity exceeded",
        [LINUX_GUEST_UNMAPPED] = "unmapped guest range",
        [LINUX_GUEST_PERMISSION] = "guest permission denied",
        [LINUX_GUEST_NOT_LOADED] = "no guest image",
        [LINUX_GUEST_UNSUPPORTED_INSTRUCTION] = "guest execution not implemented"
    };
    return (unsigned)status < sizeof(errors) / sizeof(errors[0]) ? errors[status] : "unknown error";
}
