/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "linux_exec.h"

enum { I386_EXIT = 1, I386_WRITE = 4 };
enum { I386_EIO = 5, I386_EBADF = 9, I386_EFAULT = 14, I386_ENOSYS = 38 };

static int32_t guest_write(const struct linux_guest *guest, const struct linux_i386_cpu *cpu,
                           const struct linux_guest_services *services)
{
    if (cpu->ebx != 1) return -I386_EBADF;
    if (!cpu->edx) return 0;
    const unsigned char *bytes;
    if (linux_guest_read_span(guest,cpu->ecx,cpu->edx,&bytes) != LINUX_GUEST_OK)
        return -I386_EFAULT;
    if (!services || !services->output) return -I386_EIO;

    int32_t result = services->output(services->context,bytes,cpu->edx);
    if (result < -4095 || (result >= 0 && (uint32_t)result > cpu->edx)) return -I386_EIO;
    return result;
}

enum linux_guest_status linux_guest_syscall(struct linux_guest *guest, struct linux_i386_cpu *cpu,
                                           const struct linux_guest_services *services)
{
    switch (cpu->eax) {
    case I386_EXIT:
        guest->exit_status = (uint8_t)cpu->ebx;
        guest->exited = 1;
        return LINUX_GUEST_EXITED;
    case I386_WRITE:
        cpu->eax = (uint32_t)guest_write(guest,cpu,services);
        return LINUX_GUEST_OK;
    default:
        cpu->eax = (uint32_t)-I386_ENOSYS;
        return LINUX_GUEST_OK;
    }
}
