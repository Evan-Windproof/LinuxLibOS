/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef LIBERUMS_ABI_ELF_H
#define LIBERUMS_ABI_ELF_H
#include <abi/syscall.h>
#define LIBERUMS_IMAGE_BASE 0x08000000u
#define LIBERUMS_IMAGE_END 0x08040000u
#define LIBERUMS_STACK_BASE 0x083fc000u
#define LIBERUMS_STACK_TOP 0x08400000u
#define LIBERUMS_ELF_SEGMENTS_MAX 8u
#define LIBERUMS_IMAGE_PAGES_MAX 64u
#define LIBERUMS_ELF_FILE_LIMIT 0x40000u
#define LIBERUMS_NOTE_TYPE 0x4c414249u
/* ELF note: 12-byte header, padded 9-byte name, 12-byte version/feature descriptor. */
struct liberums_abi_note {
    uint32_t namesz, descsz, type;
    char name[12];
    uint32_t major, minor, required_features;
};
_Static_assert(sizeof(struct liberums_abi_note) == 36, "public ABI note layout");
#endif
