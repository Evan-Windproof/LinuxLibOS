/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef LINUXLIBOS_KILO_PORT_H
#define LINUXLIBOS_KILO_PORT_H

#include <stdarg.h>
#include "libos.h"
#include "tty.h"

struct kilo_save {
    size_t size;
    uint32_t hash;
    int attempts;
    int error;
};

#define KILO_HASH_INIT UINT32_C(2166136261)
uint32_t kilo_hash(uint32_t hash, const void *bytes, size_t size);
int kilo_run(char *filename, struct kilo_save *save);
int kilo_format(char *buffer, size_t capacity, const char *format, ...)
    __attribute__((format(printf,3,4)));
int kilo_vformat(char *buffer, size_t capacity, const char *format, va_list args);
void *kilo_alloc(void *pointer, size_t size, const char *caller);
_Noreturn void kilo_fail(const char *operation);

#endif
