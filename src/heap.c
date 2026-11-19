/* SPDX-License-Identifier: GPL-3.0-or-later */
#include <abi/syscall.h>
#include "libos.h"
#include "posix.h"

#define HEAP_PAGES LIBERUMS_RESOURCE_DOMAIN_LIMIT
#define HEAP_BYTES (HEAP_PAGES * LIBERUMS_PAGE_SIZE)
#define HEAP_RIGHTS (RESOURCE_READ | RESOURCE_WRITE | RESOURCE_MAP | RESOURCE_RELEASE | RESOURCE_QUERY)
#define ALIGNMENT 16u

struct block {
    size_t capacity;
    size_t used;
} __attribute__((aligned(ALIGNMENT)));

_Static_assert(sizeof(struct block) == ALIGNMENT, "heap header alignment");

static struct resource_handle pages[HEAP_PAGES];
static unsigned page_count;
size_t heap_live_bytes, heap_peak_bytes, heap_failed_bytes;
unsigned heap_oom_count;

static unsigned char *const arena = (void *)RESOURCE_WINDOW;

static struct block *next(struct block *block)
{
    unsigned char *after = (unsigned char *)(block + 1) + block->capacity;
    return after == arena + HEAP_BYTES ? NULL : (struct block *)after;
}

static void merge(struct block *block)
{
    struct block *after;
    while ((after = next(block)) && !after->used)
        block->capacity += sizeof(*after) + after->capacity;
}

static void split(struct block *block, size_t capacity)
{
    if (block->capacity - capacity < sizeof(*block) + ALIGNMENT) return;
    struct block *rest = (void *)((unsigned char *)(block + 1) + capacity);
    *rest = (struct block){ .capacity = block->capacity - capacity - sizeof(*block) };
    block->capacity = capacity;
    merge(rest);
}

static struct block *find(void *pointer)
{
    if (page_count != HEAP_PAGES) return NULL;
    for (struct block *block = (void *)arena; block; block = next(block))
        if (pointer == block + 1 && block->used) return block;
    return NULL;
}

int heap_open(void)
{
    if (page_count) return RES_EBUSY;

    int32_t status = 0;
    for (unsigned i = 0; i < HEAP_PAGES; ++i) {
        struct resource_handle *page = &pages[i];
        status = liberums_call(SYS_PAGE_CLAIM,HEAP_RIGHTS,0,0,0,&page->generation);
        if (status < 0) break;
        page->index = (uint32_t)status;
        ++page_count;
        status = liberums_call(SYS_PAGE_MAP,page->index,page->generation,
                               RESOURCE_WINDOW + i * LIBERUMS_PAGE_SIZE,
                               RESOURCE_READ | RESOURCE_WRITE,0);
        if (status) break;
    }

    if (status) {
        int cleanup = heap_close();
        return cleanup ? cleanup : status;
    }
    heap_reset();
    heap_peak_bytes = heap_failed_bytes = 0;
    heap_oom_count = 0;
    return 0;
}

int heap_close(void)
{
    int first_error = 0;
    while (page_count) {
        unsigned i = page_count - 1;
        struct resource_handle page = pages[i];
        int status = liberums_call(SYS_PAGE_UNMAP,RESOURCE_WINDOW + i * LIBERUMS_PAGE_SIZE,0,0,0,0);
        if (status && status != RES_ENOENT && !first_error) first_error = status;
        status = liberums_call(SYS_PAGE_RELEASE,page.index,page.generation,0,0,0);
        if (status && status != RES_ESTALE) return status;
        --page_count;
    }
    return first_error;
}

void heap_reset(void)
{
    libos_safe_point();
    *(struct block *)arena = (struct block){ .capacity = HEAP_BYTES - sizeof(struct block) };
    heap_live_bytes = 0;
}

static void *oom(size_t size)
{
    heap_failed_bytes = size;
    ++heap_oom_count;
    errno = ENOMEM;
    return NULL;
}

static void set_used(struct block *block, size_t size)
{
    heap_live_bytes -= block->used;
    heap_live_bytes += size;
    if (heap_live_bytes > heap_peak_bytes) heap_peak_bytes = heap_live_bytes;
    block->used = size;
}

void *malloc(size_t size)
{
    if (!size) size = 1;
    if (page_count != HEAP_PAGES || size > HEAP_BYTES - sizeof(struct block)) return oom(size);
    libos_safe_point();
    size_t needed = (size + ALIGNMENT - 1) & ~(size_t)(ALIGNMENT - 1);
    for (struct block *block = (void *)arena; block; block = next(block)) {
        if (block->used || block->capacity < needed) continue;
        split(block,needed);
        set_used(block,size);
        return block + 1;
    }
    return oom(size);
}

void free(void *pointer)
{
    if (!pointer) return;
    libos_safe_point();
    struct block *block = find(pointer);
    if (!block) {
        errno = EINVAL;
        return;
    }
    set_used(block,0);
    for (block = (void *)arena; block; block = next(block))
        if (!block->used) merge(block);
}

void *realloc(void *pointer, size_t size)
{
    if (!pointer) return malloc(size);
    if (!size) {
        free(pointer);
        return NULL;
    }
    libos_safe_point();
    struct block *block = find(pointer);
    if (!block) {
        errno = EINVAL;
        return NULL;
    }
    if (size > HEAP_BYTES - sizeof(*block)) return oom(size);
    size_t needed = (size + ALIGNMENT - 1) & ~(size_t)(ALIGNMENT - 1);
    if (block->capacity < needed) merge(block);
    if (block->capacity >= needed) {
        split(block,needed);
        set_used(block,size);
        return pointer;
    }
    void *replacement = malloc(size);
    if (replacement) {
        memcpy(replacement,pointer,block->used);
        free(pointer);
    }
    return replacement;
}

void *calloc(size_t count, size_t size)
{
    if (size && count > SIZE_MAX / size) return oom(SIZE_MAX);
    size *= count;
    void *buffer = malloc(size);
    return buffer ? memset(buffer,0,size) : NULL;
}
