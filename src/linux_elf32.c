/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "linux_elf32.h"

#define ELF32_EHSIZE 52u
#define ELF32_MAX_PHNUM 16u

enum {
    PT_NULL = 0,
    PT_LOAD = 1,
    PT_NOTE = 4,
    PT_PHDR = 6,
    PT_GNU_STACK = 0x6474e551
};

static uint16_t read16(const unsigned char *byte)
{
    return (uint16_t)(byte[0] | (uint16_t)byte[1] << 8);
}

static uint32_t read32(const unsigned char *byte)
{
    return byte[0] | (uint32_t)byte[1] << 8 |
        (uint32_t)byte[2] << 16 | (uint32_t)byte[3] << 24;
}

static int within_file(size_t size, uint32_t offset, uint32_t length)
{
    return offset <= size && length <= size - offset;
}

static uint32_t page_start(uint32_t value)
{
    return value & ~(LINUX_GUEST_PAGE_SIZE - 1);
}

static uint32_t page_end(uint32_t value)
{
    return page_start(value + LINUX_GUEST_PAGE_SIZE - 1);
}

static enum linux_guest_status add_segment(struct linux_elf_layout *layout, uint32_t *used,
                                           const unsigned char *ph)
{
    uint32_t offset = read32(ph + 4);
    uint32_t address = read32(ph + 8);
    uint32_t filesz = read32(ph + 16);
    uint32_t memsz = read32(ph + 20);
    uint32_t flags = read32(ph + 24);
    uint32_t align = read32(ph + 28);
    uint32_t all = LINUX_PF_R | LINUX_PF_W | LINUX_PF_X;

    if (!memsz || filesz > memsz || (flags & ~all) ||
        (flags & (LINUX_PF_W | LINUX_PF_X)) == (LINUX_PF_W | LINUX_PF_X) ||
        address < LINUX_GUEST_IMAGE_MIN || address >= LINUX_GUEST_IMAGE_END ||
        memsz > LINUX_GUEST_IMAGE_END - address)
        return LINUX_GUEST_SEGMENT;

    uint32_t page_mask = LINUX_GUEST_PAGE_SIZE - 1;
    if ((address & page_mask) != (offset & page_mask) ||
        (align > 1 && ((align & (align - 1)) || (address & (align - 1)) != (offset & (align - 1)))))
        return LINUX_GUEST_ALIGNMENT;

    if (layout->count == LINUX_GUEST_MAX_SEGMENTS || memsz > LINUX_GUEST_IMAGE_SIZE - *used)
        return LINUX_GUEST_CAPACITY;

    for (unsigned i = 0; i < layout->count; ++i) {
        const struct linux_guest_region *other = &layout->segments[i].region;
        if (page_start(address) < page_end(other->base + other->size) &&
            page_start(other->base) < page_end(address + memsz))
            return LINUX_GUEST_OVERLAP;
    }

    layout->segments[layout->count++] = (struct linux_elf_segment){
        .region = { address, memsz, *used, flags },
        .file_offset = offset, .file_size = filesz
    };
    *used += memsz;
    return LINUX_GUEST_OK;
}

enum linux_guest_status linux_elf32_parse(struct linux_elf_layout *layout,
                                         const unsigned char *file, size_t size)
{
    if (size > LINUX_ELF_MAX_FILE) return LINUX_GUEST_CAPACITY;
    if (size < ELF32_EHSIZE) return LINUX_GUEST_TRUNCATED;
    if (file[0] != 0x7f || file[1] != 'E' || file[2] != 'L' || file[3] != 'F')
        return LINUX_GUEST_FORMAT;
    /* ELFCLASS32, ELFDATA2LSB, ET_EXEC, EM_386, System V/Linux OSABI. */
    if (file[4] != 1 || file[5] != 1 || (file[7] != 0 && file[7] != 3) ||
        file[8] != 0 || read16(file + 16) != 2 || read16(file + 18) != 3 ||
        read32(file + 36) != 0)
        return LINUX_GUEST_UNSUPPORTED_ELF;
    if (file[6] != 1 || read32(file + 20) != 1 || read16(file + 40) != ELF32_EHSIZE ||
        read16(file + 42) != ELF32_PHENT)
        return LINUX_GUEST_FORMAT;

    uint32_t phoff = read32(file + 28);
    uint32_t phnum = read16(file + 44);
    if (!phnum || phoff < ELF32_EHSIZE) return LINUX_GUEST_FORMAT;
    if (phnum > ELF32_MAX_PHNUM) return LINUX_GUEST_CAPACITY;
    uint32_t table_size = phnum * ELF32_PHENT;
    if (!within_file(size,phoff,table_size)) return LINUX_GUEST_TRUNCATED;

    *layout = (struct linux_elf_layout){ .entry = read32(file + 24), .phnum = phnum };
    uint32_t used = 0, declared_phdr = 0;
    int has_phdr = 0, has_stack = 0;
    for (uint32_t i = 0; i < phnum; ++i) {
        const unsigned char *ph = file + phoff + i * ELF32_PHENT;
        uint32_t type = read32(ph);
        if (type == PT_NULL) continue;
        if (type != PT_LOAD && type != PT_NOTE && type != PT_PHDR && type != PT_GNU_STACK)
            return LINUX_GUEST_UNSUPPORTED_ELF;

        uint32_t offset = read32(ph + 4);
        uint32_t filesz = read32(ph + 16);
        if (!within_file(size,offset,filesz)) return LINUX_GUEST_TRUNCATED;
        if (type == PT_LOAD) {
            enum linux_guest_status status = add_segment(layout,&used,ph);
            if (status != LINUX_GUEST_OK) return status;
        } else if (type == PT_PHDR) {
            if (has_phdr || offset != phoff || filesz != table_size ||
                read32(ph + 20) != table_size) return LINUX_GUEST_FORMAT;
            has_phdr = 1;
            declared_phdr = read32(ph + 8);
        } else if (type == PT_GNU_STACK) {
            if (has_stack || filesz || read32(ph + 20)) return LINUX_GUEST_FORMAT;
            if (read32(ph + 24) & ~(uint32_t)(LINUX_PF_R | LINUX_PF_W))
                return LINUX_GUEST_UNSUPPORTED_ELF;
            has_stack = 1;
        }
    }

    int entry_valid = 0;
    for (unsigned i = 0; i < layout->count; ++i) {
        const struct linux_elf_segment *segment = &layout->segments[i];
        const struct linux_guest_region *region = &segment->region;
        if ((region->flags & LINUX_PF_X) && layout->entry >= region->base &&
            layout->entry - region->base < segment->file_size) entry_valid = 1;

        if ((region->flags & LINUX_PF_R) && phoff >= segment->file_offset &&
            within_file(segment->file_size,phoff - segment->file_offset,table_size)) {
            uint32_t address = region->base + (phoff - segment->file_offset);
            if (!layout->phdr || (has_phdr && address == declared_phdr)) layout->phdr = address;
        }
    }
    if (has_phdr && (!layout->phdr || layout->phdr != declared_phdr)) return LINUX_GUEST_FORMAT;
    return entry_valid ? LINUX_GUEST_OK : LINUX_GUEST_ENTRY;
}
