/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "libos.h"

int libos_main(void)
{
    if (heap_open()) return 80;
    unsigned char *page = malloc(32);
    if (!page) return 81;
    *page = 0xa5;
    libos_observe(2);
    __asm__ volatile ("ud2");
    return 82;
}
