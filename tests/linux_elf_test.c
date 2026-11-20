/* SPDX-License-Identifier: GPL-3.0-or-later */
#include <assert.h>
#include <elf.h>
#include <stdio.h>
#include <string.h>
#include "linux_guest.h"

static struct linux_guest guest, snapshot;
static unsigned char input[LINUX_ELF_MAX_FILE], modified[LINUX_ELF_MAX_FILE];
static unsigned char segment_bytes[LINUX_GUEST_IMAGE_SIZE];
static size_t input_size;
static Elf32_Ehdr header;
static Elf32_Phdr code, rodata, data;
static size_t code_ph, rodata_ph, data_ph, stack_ph;
static uint32_t phdr_address;
static const char *const args[] = { "linux-sample", "phase3.1" };
static const char *const env[] = { "LANG=C" };

static enum linux_guest_status load(const void *file, size_t size)
{
    return linux_guest_load_elf(&guest,file,size,2,args,1,env);
}

static void reject(size_t at, unsigned width, uint32_t value, enum linux_guest_status status)
{
    memcpy(modified,input,input_size);
    for (unsigned i = 0; i < width; ++i) {
        modified[at+i] = (unsigned char)value;
        value >>= 8;
    }
    assert(load(modified,input_size) == status);
    assert(!memcmp(&guest,&snapshot,sizeof(guest)));
}

#define WORD(ph, field) ((ph) + offsetof(Elf32_Phdr,field))

static void segments(void)
{
    unsigned loads = 0;
    for (unsigned i = 0; i < header.e_phnum; ++i) {
        size_t at = header.e_phoff + i * header.e_phentsize;
        Elf32_Phdr p;
        memcpy(&p,input + at,sizeof(p));
        if (p.p_type == PT_GNU_STACK) stack_ph = at;
        if (p.p_type != PT_LOAD) continue;
        const struct linux_guest_region *region = &guest.regions[loads++];
        assert(region->base == p.p_vaddr && region->size == p.p_memsz && region->flags == p.p_flags);
        assert(linux_guest_read(&guest,p.p_vaddr,segment_bytes,p.p_memsz) == LINUX_GUEST_OK);
        assert(!memcmp(segment_bytes,input + p.p_offset,p.p_filesz));
        for (size_t j = p.p_filesz; j < p.p_memsz; ++j) assert(!segment_bytes[j]);
        if ((p.p_flags & PF_R) && p.p_offset <= header.e_phoff &&
            header.e_phoff + header.e_phnum * sizeof(p) <= p.p_offset + p.p_filesz)
            phdr_address = p.p_vaddr + header.e_phoff - p.p_offset;
        if (p.p_flags == (PF_R | PF_X)) { code = p; code_ph = at; }
        if (p.p_flags == (PF_R | PF_W)) { data = p; data_ph = at; }
        if (p.p_flags == PF_R && p.p_offset) { rodata = p; rodata_ph = at; }
    }
    assert(loads == guest.region_count && loads >= 3);
    assert(code_ph && rodata_ph && data_ph && stack_ph && phdr_address && data.p_memsz > data.p_filesz);
}

static uint32_t word(uint32_t address)
{
    unsigned char b[4];
    assert(linux_guest_read(&guest,address,b,sizeof(b)) == LINUX_GUEST_OK);
    return b[0] | (uint32_t)b[1] << 8 | (uint32_t)b[2] << 16 | (uint32_t)b[3] << 24;
}

static void stack(void)
{
    uint32_t esp = guest.cpu.esp;
    assert(esp >= LINUX_GUEST_STACK_TOP - LINUX_GUEST_STACK_SIZE && !(esp & 15));
    assert(guest.cpu.eip == header.e_entry && guest.cpu.eflags == 0x202);
    assert(!guest.cpu.eax && !guest.cpu.ebx && !guest.cpu.ecx && !guest.cpu.edx &&
           !guest.cpu.esi && !guest.cpu.edi && !guest.cpu.ebp);
    assert(word(esp) == 2 && !word(esp+12) && !word(esp+20));
    const uint32_t pointers[] = { word(esp+4), word(esp+8), word(esp+16) };
    const char *const strings[] = { args[0], args[1], env[0] };
    for (size_t i = 0; i < 3; ++i) {
        char result[24];
        assert(pointers[i] > esp + 68 && pointers[i] < LINUX_GUEST_STACK_TOP);
        size_t length = strlen(strings[i]) + 1;
        assert(linux_guest_read(&guest,pointers[i],result,length) == LINUX_GUEST_OK);
        assert(!memcmp(result,strings[i],length));
    }

    const uint32_t aux[] = { AT_PHDR, phdr_address,
                             AT_PHENT, sizeof(Elf32_Phdr), AT_PHNUM, header.e_phnum,
                             AT_PAGESZ, 4096, AT_ENTRY, header.e_entry, AT_NULL, 0 };
    for (size_t i = 0; i < sizeof(aux)/sizeof(aux[0]); ++i)
        assert(word(esp + 24 + (uint32_t)i * 4) == aux[i]);
    assert(linux_guest_read(&guest,aux[1],segment_bytes,header.e_phnum * sizeof(Elf32_Phdr)) ==
           LINUX_GUEST_OK);
    assert(!memcmp(segment_bytes,input + header.e_phoff,header.e_phnum * sizeof(Elf32_Phdr)));
}

static void access_checks(void)
{
    unsigned char byte = 0;
    assert(linux_guest_fetch(&guest,guest.cpu.eip,&byte,1) == LINUX_GUEST_OK);
    assert(byte == input[code.p_offset + header.e_entry - code.p_vaddr]);
    assert(guest.cpu.eip == header.e_entry && guest.cpu.eax == 0);
    assert(linux_guest_write(&guest,guest.cpu.eip,&byte,1) == LINUX_GUEST_PERMISSION);
    assert(linux_guest_write(&guest,rodata.p_vaddr,&byte,1) == LINUX_GUEST_PERMISSION);
    assert(linux_guest_fetch(&guest,guest.cpu.esp,&byte,1) == LINUX_GUEST_PERMISSION);
    assert(linux_guest_fetch(&guest,data.p_vaddr,&byte,1) == LINUX_GUEST_PERMISSION);
    assert(linux_guest_read(&guest,code.p_vaddr+code.p_memsz-1,&byte,2) == LINUX_GUEST_UNMAPPED);
    assert(linux_guest_read(&guest,LINUX_GUEST_STACK_TOP-1,&byte,2) == LINUX_GUEST_UNMAPPED);
    assert(linux_guest_read(&guest,code.p_vaddr,&byte,SIZE_MAX) == LINUX_GUEST_UNMAPPED);
    assert(linux_guest_read(&guest,UINT32_MAX,&byte,2) == LINUX_GUEST_UNMAPPED);
    assert(linux_guest_read(&guest,0,&byte,1) == LINUX_GUEST_UNMAPPED);
    assert(linux_guest_read(&guest,code.p_vaddr,&byte,0) == LINUX_GUEST_ARGUMENT);

    byte = 0x5a;
    assert(linux_guest_write(&guest,data.p_vaddr+data.p_filesz,&byte,1) == LINUX_GUEST_OK);
    uint8_t result = 0;
    assert(linux_guest_read(&guest,data.p_vaddr+data.p_filesz,&result,1) == LINUX_GUEST_OK);
    assert(result == byte);
    assert(linux_guest_write(&guest,LINUX_GUEST_STACK_TOP-1,&byte,1) == LINUX_GUEST_OK);
    assert(linux_guest_read(&guest,LINUX_GUEST_STACK_TOP-1,&result,1) == LINUX_GUEST_OK && result == byte);
    guest.cpu.eip = guest.cpu.esp;
    assert(linux_guest_fetch(&guest,guest.cpu.eip,&byte,1) == LINUX_GUEST_PERMISSION && byte == 0x5a);
    assert(guest.cpu.eip == guest.cpu.esp);

    assert(load(input,input_size) == LINUX_GUEST_OK);
    assert(linux_guest_read(&guest,data.p_vaddr+data.p_filesz,&result,1) == LINUX_GUEST_OK && !result);

    /* Synthetic permission variant. The actual guest fixture stays unchanged. */
    memcpy(modified,input,input_size);
    modified[WORD(code_ph,p_flags)] = PF_X;
    assert(load(modified,input_size) == LINUX_GUEST_OK);
    assert(linux_guest_fetch(&guest,guest.cpu.eip,&byte,1) == LINUX_GUEST_OK);
    assert(linux_guest_read(&guest,guest.cpu.eip,&byte,1) == LINUX_GUEST_PERMISSION);
    assert(load(input,input_size) == LINUX_GUEST_OK);
}

static void malformed(void)
{
    memcpy(&snapshot,&guest,sizeof(guest));
    reject(EI_MAG0,1,0,LINUX_GUEST_FORMAT);
    reject(EI_CLASS,1,ELFCLASS64,LINUX_GUEST_UNSUPPORTED_ELF);
    reject(EI_DATA,1,ELFDATA2MSB,LINUX_GUEST_UNSUPPORTED_ELF);
    reject(offsetof(Elf32_Ehdr,e_machine),2,EM_ARM,LINUX_GUEST_UNSUPPORTED_ELF);
    reject(offsetof(Elf32_Ehdr,e_type),2,ET_DYN,LINUX_GUEST_UNSUPPORTED_ELF);
    reject(code_ph,4,PT_INTERP,LINUX_GUEST_UNSUPPORTED_ELF);
    reject(code_ph,4,PT_DYNAMIC,LINUX_GUEST_UNSUPPORTED_ELF);
    reject(WORD(stack_ph,p_flags),4,PF_R | PF_W | PF_X,LINUX_GUEST_UNSUPPORTED_ELF);
    reject(offsetof(Elf32_Ehdr,e_phentsize),2,0,LINUX_GUEST_FORMAT);
    reject(offsetof(Elf32_Ehdr,e_phoff),4,UINT32_MAX,LINUX_GUEST_TRUNCATED);
    reject(offsetof(Elf32_Ehdr,e_phoff),4,(uint32_t)input_size-4,LINUX_GUEST_TRUNCATED);
    reject(WORD(code_ph,p_offset),4,UINT32_MAX-1,LINUX_GUEST_TRUNCATED);
    reject(WORD(code_ph,p_filesz),4,UINT32_MAX,LINUX_GUEST_TRUNCATED);
    reject(WORD(code_ph,p_memsz),4,code.p_filesz-1,LINUX_GUEST_SEGMENT);
    reject(WORD(code_ph,p_memsz),4,UINT32_MAX,LINUX_GUEST_SEGMENT);
    reject(WORD(code_ph,p_vaddr),4,0,LINUX_GUEST_SEGMENT);
    reject(WORD(data_ph,p_vaddr),4,LINUX_GUEST_STACK_TOP,LINUX_GUEST_SEGMENT);
    reject(WORD(code_ph,p_align),4,3,LINUX_GUEST_ALIGNMENT);
    reject(WORD(code_ph,p_offset),4,code.p_offset+1,LINUX_GUEST_ALIGNMENT);
    reject(WORD(rodata_ph,p_vaddr),4,code.p_vaddr,LINUX_GUEST_OVERLAP);
    reject(WORD(code_ph,p_flags),4,PF_R | PF_W | PF_X,LINUX_GUEST_SEGMENT);
    reject(WORD(data_ph,p_memsz),4,LINUX_GUEST_IMAGE_SIZE,LINUX_GUEST_CAPACITY);
    reject(offsetof(Elf32_Ehdr,e_entry),4,rodata.p_vaddr,LINUX_GUEST_ENTRY);
    reject(offsetof(Elf32_Ehdr,e_entry),4,code.p_vaddr+code.p_filesz,LINUX_GUEST_ENTRY);
    assert(load(input,sizeof(Elf32_Ehdr)-1) == LINUX_GUEST_TRUNCATED);
    assert(load(input,sizeof(input)+1) == LINUX_GUEST_CAPACITY);
    assert(linux_guest_load_elf(&guest,input,input_size,9,args,0,NULL) == LINUX_GUEST_STACK);

    char long_string[256];
    memset(long_string,'a',sizeof(long_string));
    const char *long_args[8];
    for (size_t i = 0; i < 8; ++i) long_args[i] = long_string;
    assert(linux_guest_load_elf(&guest,input,input_size,1,long_args,0,NULL) == LINUX_GUEST_STACK);
    long_string[255] = '\0';
    assert(linux_guest_load_elf(&guest,input,input_size,8,long_args,8,long_args) == LINUX_GUEST_STACK);
    assert(!memcmp(&guest,&snapshot,sizeof(guest)));
}

int main(int argc, char *argv[])
{
    assert(argc == 2);
    FILE *file = fopen(argv[1],"rb");
    assert(file);
    input_size = fread(input,1,sizeof(input),file);
    assert(input_size >= sizeof(header) && feof(file) && !ferror(file) && !fclose(file));
    memcpy(&header,input,sizeof(header));

    unsigned char byte;
    assert(linux_guest_read(&guest,0,&byte,1) == LINUX_GUEST_NOT_LOADED);
    assert(load(input,input_size) == LINUX_GUEST_OK);
    segments();
    stack();
    access_checks();
    malformed();
    puts("PASS: independent Linux ET_EXEC, LOAD/BSS, address permissions, initial stack and rejection cases");
    return 0;
}
