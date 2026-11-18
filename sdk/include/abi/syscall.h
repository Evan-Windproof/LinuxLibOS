/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef LIBERUMS_ABI_SYSCALL_H
#define LIBERUMS_ABI_SYSCALL_H
#include <stdint.h>
#include <abi/resource.h>
#include <abi/event.h>
#define SYS_YIELD 0u
#define SYS_EXIT 1u
#define SYS_LOG_CHAR 2u
#define SYS_ABI_QUERY 3u
#define SYS_RESOURCE_LIMITS 4u
#define LIBERUMS_ABI_MAJOR 1u
#define LIBERUMS_ABI_MINOR 2u
#define LIBERUMS_FEATURE_PAGES 1u
#define LIBERUMS_FEATURE_TRANSFER 2u
#define LIBERUMS_FEATURE_ELF32 4u
#define LIBERUMS_FEATURE_COOPERATIVE 8u
#define LIBERUMS_FEATURE_LIMITS 16u
#define LIBERUMS_FEATURE_PREEMPTION 32u
#define LIBERUMS_FEATURE_REVOCATION 64u
/* Active execution profile: COOPERATIVE means IF=0 and explicit yield only. */
#define LIBERUMS_FEATURES 31u
#define LIBERUMS_PREEMPT_FEATURES ((LIBERUMS_FEATURES & ~LIBERUMS_FEATURE_COOPERATIVE) \
    | LIBERUMS_FEATURE_PREEMPTION | LIBERUMS_FEATURE_REVOCATION)
#define LIBERUMS_ABI_VERSION ((LIBERUMS_ABI_MAJOR << 16) | LIBERUMS_ABI_MINOR)

/* A newer minor is compatible if every requested feature is present. */
static inline int liberums_abi_compatible(uint32_t version, uint32_t features,
                                         uint32_t major, uint32_t minor, uint32_t required)
{
    return (version >> 16) == major && (version & 0xffffu) >= minor
        && (features & required) == required;
}

/* Only integer registers cross the boundary. extra is a user-local output pointer. */
static inline int32_t liberums_call(uint32_t number, uint32_t bx, uint32_t cx,
                                    uint32_t dx, uint32_t si, uint32_t *extra)
{
    __asm__ volatile ("int $0x80" : "+a"(number), "+d"(dx)
                      : "b"(bx), "c"(cx), "S"(si) : "memory", "cc");
    if (extra != 0) {
        *extra = dx;
    }
    return (int32_t)number;
}
static inline _Noreturn void liberums_exit(uint32_t status)
{
    liberums_call(SYS_EXIT, status, 0, 0, 0, 0);
    for (;;) { __asm__ volatile ("ud2"); }
}
#endif
