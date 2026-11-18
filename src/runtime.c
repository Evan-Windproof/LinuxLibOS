/* SPDX-License-Identifier: GPL-3.0-or-later */
#include <abi/elf.h>
#include "libos.h"

#define REQUIRED_FEATURES (LIBERUMS_FEATURE_PAGES | LIBERUMS_FEATURE_ELF32 | \
                           LIBERUMS_FEATURE_LIMITS | LIBERUMS_FEATURE_PREEMPTION | \
                           LIBERUMS_FEATURE_REVOCATION)

__attribute__((used,section(".note.liberums"),aligned(4)))
static const struct liberums_abi_note note = {
    9, 12, LIBERUMS_NOTE_TYPE, "LIBERUMS", LIBERUMS_ABI_MAJOR,
    LIBERUMS_ABI_MINOR, REQUIRED_FEATURES
};

uint32_t libos_version, libos_features, libos_page_limit, libos_binding_limit;
volatile unsigned libos_stage;

__attribute__((noinline,noipa))
void libos_observe(unsigned stage)
{
    libos_stage = stage;
}

void libos_log(const char *message)
{
    while (*message && !liberums_call(SYS_LOG_CHAR,(unsigned char)*message,0,0,0,0))
        ++message;
}

_Noreturn void libos_exit(unsigned status)
{
    liberums_exit(status);
}

void libos_safe_point(void)
{
    uint32_t request, index, generation;
    int32_t status = liberums_call(SYS_REVOKE_POLL,0,0,0,0,&request);
    if (status == RES_ENOENT) return;
    /* The session ends here, so no editor object will be used after ACK.
       Do not touch the arena or unwind editor state. */
    if (!status && !liberums_call(SYS_REVOKE_INFO,request,REVOKE_INFO_INDEX,0,0,&index) &&
        !liberums_call(SYS_REVOKE_INFO,request,REVOKE_INFO_GENERATION,0,0,&generation))
        liberums_call(SYS_REVOKE_ACK,index,generation,request,0,0);
    libos_log("LinuxLibOS: resource event, ending session\n");
    libos_exit(96);
}

_Noreturn void libos_start(void)
{
    int32_t version = liberums_call(SYS_ABI_QUERY,0,0,0,0,&libos_features);
    if (version < 0 || !liberums_abi_compatible((uint32_t)version,libos_features,
                                               LIBERUMS_ABI_MAJOR,LIBERUMS_ABI_MINOR,
                                               REQUIRED_FEATURES)) {
        libos_log("LinuxLibOS: incompatible ABI\n");
        libos_exit(90);
    }

    int32_t limit = liberums_call(SYS_RESOURCE_LIMITS,0,0,0,0,&libos_binding_limit);
    if (limit < 1) libos_exit(91);
    libos_version = (uint32_t)version;
    libos_page_limit = (uint32_t)limit;
    libos_log("LinuxLibOS: ABI 1.2 ready\n");
    libos_observe(1);
    libos_exit((unsigned)libos_main());
}
