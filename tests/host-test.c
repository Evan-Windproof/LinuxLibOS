/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "ramfs_cases.h"
#include "tty.h"

int tty_ready(void) { return 0; }

ssize_t tty_read(void *buffer, size_t size)
{
    (void)buffer;
    (void)size;
    errno = ENOTTY;
    return -1;
}

ssize_t tty_write(const void *buffer, size_t size)
{
    (void)buffer;
    (void)size;
    errno = ENOTTY;
    return -1;
}

int main(void)
{
    return ramfs_tests();
}
