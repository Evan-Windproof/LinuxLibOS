/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef LINUXLIBOS_RUNTIME_H
#define LINUXLIBOS_RUNTIME_H

#include <stddef.h>
#include <stdint.h>

void libos_log(const char *message);
_Noreturn void libos_exit(unsigned status);
void libos_safe_point(void);

int heap_open(void);
int heap_close(void);
extern size_t heap_peak_bytes;
extern size_t heap_failed_bytes;
extern unsigned heap_oom_count;

void *malloc(size_t size);
void *calloc(size_t count, size_t size);
void *realloc(void *pointer, size_t size);
void free(void *pointer);

void *memcpy(void *restrict dest, const void *restrict source, size_t size);
void *memmove(void *dest, const void *source, size_t size);
void *memset(void *dest, int byte, size_t size);
int memcmp(const void *a, const void *b, size_t size);
size_t strlen(const char *string);
int strncmp(const char *a, const char *b, size_t size);
char *strchr(const char *string, int byte);
char *strstr(const char *string, const char *needle);
int isspace(int byte);
int isdigit(int byte);
int isprint(int byte);

int libos_main(void);
void libos_observe(unsigned stage);
extern volatile unsigned libos_stage;

#endif
