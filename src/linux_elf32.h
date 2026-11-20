/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef LINUXLIBOS_ELF32_INTERNAL_H
#define LINUXLIBOS_ELF32_INTERNAL_H

#include "linux_guest.h"

struct linux_elf_segment {
    struct linux_guest_region region;
    uint32_t file_offset, file_size;
};

struct linux_elf_layout {
    struct linux_elf_segment segments[LINUX_GUEST_MAX_SEGMENTS];
    uint32_t entry, phdr, phnum;
    unsigned count;
};

#define ELF32_PHENT 32u

enum linux_guest_status linux_elf32_parse(struct linux_elf_layout *layout,
                                         const unsigned char *file, size_t size);

#endif
