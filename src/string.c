/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "libos.h"

void *memcpy(void *restrict dest, const void *restrict source, size_t size)
{
    unsigned char *out = dest;
    const unsigned char *in = source;
    for (size_t i = 0; i < size; ++i) out[i] = in[i];
    return dest;
}

void *memmove(void *dest, const void *source, size_t size)
{
    unsigned char *out = dest;
    const unsigned char *in = source;
    if ((uintptr_t)out <= (uintptr_t)in) {
        for (size_t i = 0; i < size; ++i) out[i] = in[i];
    } else {
        while (size) {
            --size;
            out[size] = in[size];
        }
    }
    return dest;
}

void *memset(void *dest, int byte, size_t size)
{
    unsigned char *out = dest;
    for (size_t i = 0; i < size; ++i) out[i] = (unsigned char)byte;
    return dest;
}

int memcmp(const void *a, const void *b, size_t size)
{
    const unsigned char *left = a, *right = b;
    for (size_t i = 0; i < size; ++i)
        if (left[i] != right[i]) return left[i] - right[i];
    return 0;
}

size_t strlen(const char *string)
{
    size_t size = 0;
    while (string[size]) ++size;
    return size;
}

int strncmp(const char *a, const char *b, size_t size)
{
    for (size_t i = 0; i < size; ++i) {
        if (a[i] != b[i]) return (unsigned char)a[i] - (unsigned char)b[i];
        if (!a[i]) break;
    }
    return 0;
}

char *strchr(const char *string, int byte)
{
    for (; *string != (char)byte; ++string)
        if (!*string) return NULL;
    return (char *)string;
}

char *strstr(const char *string, const char *needle)
{
    size_t size = strlen(needle);
    if (!size) return (char *)string;
    for (; *string; ++string)
        if (!strncmp(string,needle,size)) return (char *)string;
    return NULL;
}

int isspace(int byte) { return byte == ' ' || (byte >= '\t' && byte <= '\r'); }
int isdigit(int byte) { return byte >= '0' && byte <= '9'; }
int isprint(int byte) { return byte >= ' ' && byte <= '~'; }
