/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef LINUXLIBOS_TTY_H
#define LINUXLIBOS_TTY_H

#include "posix.h"

int tty_open(void);
int tty_close(void);
int tty_ready(void);
ssize_t tty_read(void *buffer, size_t count);
ssize_t tty_write(const void *buffer, size_t count);
ssize_t tty_escape_byte(int fd, char *byte);

#endif
