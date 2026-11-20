/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef LINUXLIBOS_LINUX_GUEST_H
#define LINUXLIBOS_LINUX_GUEST_H

#include <stddef.h>
#include <stdint.h>

#define LINUX_ELF_MAX_FILE (32u * 1024u)
#define LINUX_GUEST_IMAGE_SIZE (32u * 1024u)
#define LINUX_GUEST_PAGE_SIZE 4096u
#define LINUX_GUEST_STACK_SIZE LINUX_GUEST_PAGE_SIZE
#define LINUX_GUEST_STACK_TOP UINT32_C(0xc0000000)
#define LINUX_GUEST_IMAGE_MIN UINT32_C(0x00010000)
#define LINUX_GUEST_IMAGE_END UINT32_C(0xbff00000)
#define LINUX_GUEST_MAX_SEGMENTS 8u
#define LINUX_GUEST_STEP_LIMIT 4096u

/* Same bit values as ELF p_flags. */
enum { LINUX_PF_X = 1, LINUX_PF_W = 2, LINUX_PF_R = 4 };

enum linux_guest_status {
    LINUX_GUEST_OK,
    LINUX_GUEST_ARGUMENT,
    LINUX_GUEST_FORMAT,
    LINUX_GUEST_UNSUPPORTED_ELF,
    LINUX_GUEST_TRUNCATED,
    LINUX_GUEST_SEGMENT,
    LINUX_GUEST_ALIGNMENT,
    LINUX_GUEST_OVERLAP,
    LINUX_GUEST_ENTRY,
    LINUX_GUEST_CAPACITY,
    LINUX_GUEST_STACK,
    LINUX_GUEST_UNMAPPED,
    LINUX_GUEST_PERMISSION,
    LINUX_GUEST_NOT_LOADED,
    LINUX_GUEST_UNSUPPORTED_INSTRUCTION,
    LINUX_GUEST_TRUNCATED_INSTRUCTION,
    LINUX_GUEST_ENCODING,
    LINUX_GUEST_BAD_JUMP,
    LINUX_GUEST_STEP_EXHAUSTED,
    LINUX_GUEST_EXITED
};

struct linux_i386_cpu {
    uint32_t eax, ebx, ecx, edx, esi, edi, ebp, esp, eip, eflags;
};

struct linux_guest_region {
    uint32_t base, size, offset, flags;
};

/* Native, trusted state. Neither the metadata nor the CPU is guest-addressable. */
struct linux_guest {
    struct linux_i386_cpu cpu;
    struct linux_guest_region regions[LINUX_GUEST_MAX_SEGMENTS];
    unsigned region_count;
    unsigned steps;
    uint8_t exited, exit_status;
    unsigned char image[LINUX_GUEST_IMAGE_SIZE];
    unsigned char stack[LINUX_GUEST_STACK_SIZE];
};

/* Zero-initialize before first use. Native inputs must not overlap the guest.
   A rejected load leaves any existing image, stack and registers untouched. */
enum linux_guest_status linux_guest_load_elf(struct linux_guest *guest,
                                            const void *file, size_t size,
                                            size_t argc, const char *const argv[],
                                            size_t envc, const char *const envp[]);
enum linux_guest_status linux_guest_read(const struct linux_guest *guest, uint32_t address,
                                        void *buffer, size_t size);
enum linux_guest_status linux_guest_write(struct linux_guest *guest, uint32_t address,
                                         const void *buffer, size_t size);
enum linux_guest_status linux_guest_fetch(const struct linux_guest *guest, uint32_t address,
                                         void *buffer, size_t size);

struct linux_guest_services {
    /* Called synchronously with a validated native span. Return bytes written
       or a negative Linux errno. Do not retain the span or mutate the guest. */
    int32_t (*output)(void *context, const unsigned char *bytes, size_t size);
    void (*safe_point)(void *context);
    void *context;
};

/* Commit one successful instruction, including a virtual exit. Other errors
   leave the CPU, memory and completed-step count unchanged. */
enum linux_guest_status linux_guest_step(struct linux_guest *guest,
                                        const struct linux_guest_services *services);
enum linux_guest_status linux_guest_run(struct linux_guest *guest,
                                       const struct linux_guest_services *services);
const char *linux_guest_error(enum linux_guest_status status);

#endif
