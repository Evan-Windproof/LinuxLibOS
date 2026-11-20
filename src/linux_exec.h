/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef LINUXLIBOS_EXEC_INTERNAL_H
#define LINUXLIBOS_EXEC_INTERNAL_H

#include "linux_guest.h"

enum linux_guest_status linux_guest_read_span(const struct linux_guest *guest, uint32_t address,
                                             size_t size, const unsigned char **data);
enum linux_guest_status linux_guest_syscall(struct linux_guest *guest, struct linux_i386_cpu *cpu,
                                           const struct linux_guest_services *services);

#endif
