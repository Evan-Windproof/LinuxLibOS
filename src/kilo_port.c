/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "kilo_port.h"

static void append(char *buffer, size_t capacity, size_t *size, char byte)
{
    if (*size + 1 < capacity) buffer[*size] = byte;
    ++*size;
}

int kilo_vformat(char *buffer, size_t capacity, const char *format, va_list args)
{
    size_t size = 0;
    for (; *format; ++format) {
        if (*format != '%') {
            append(buffer,capacity,&size,*format);
            continue;
        }
        ++format;
        unsigned precision = UINT32_MAX;
        if (*format == '.') {
            precision = 0;
            while (isdigit(*++format))
                precision = precision * 10 + (unsigned)(*format - '0');
        }

        switch (*format) {
        case 's': {
            const char *value = va_arg(args,const char *);
            while (precision && *value) {
                append(buffer,capacity,&size,*value++);
                --precision;
            }
            break;
        }
        case 'd': {
            int value = va_arg(args,int);
            unsigned magnitude = (unsigned)value;
            if (value < 0) {
                append(buffer,capacity,&size,'-');
                magnitude = 0u - magnitude;
            }
            char digits[10];
            unsigned count = 0;
            do {
                digits[count++] = (char)('0' + magnitude % 10);
                magnitude /= 10;
            } while (magnitude);
            while (count) append(buffer,capacity,&size,digits[--count]);
            break;
        }
        case '%':
            append(buffer,capacity,&size,'%');
            break;
        default:
            if (capacity) buffer[0] = '\0';
            errno = EINVAL;
            return -1;
        }
    }
    if (capacity) buffer[size < capacity ? size : capacity - 1] = '\0';
    return (int)size;
}

int kilo_format(char *buffer, size_t capacity, const char *format, ...)
{
    va_list args;
    va_start(args,format);
    int result = kilo_vformat(buffer,capacity,format,args);
    va_end(args);
    return result;
}

_Noreturn void kilo_fail(const char *operation)
{
    char diagnostic[96];
    kilo_format(diagnostic,sizeof(diagnostic),"Kilo %.32s: %.32s\n",operation,strerror(errno));
    libos_log(diagnostic);
    libos_exit(70);
}

void *kilo_alloc(void *pointer, size_t size, const char *caller)
{
    if (!size) {
        free(pointer);
        return NULL;
    }
    void *result = realloc(pointer,size);
    if (!result) {
        char diagnostic[96];
        kilo_format(diagnostic,sizeof(diagnostic),"Kilo OOM: %.32s requested %d bytes\n",caller,(int)size);
        libos_log(diagnostic);
        libos_exit(71);
    }
    return result;
}
