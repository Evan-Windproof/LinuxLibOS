/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef LIBERUMS_ABI_RESOURCE_H
#define LIBERUMS_ABI_RESOURCE_H
#include <stdint.h>
#define LIBERUMS_PAGE_SIZE 4096u
#define SYS_PAGE_CLAIM 16u
#define SYS_PAGE_SPECIFIC 17u
#define SYS_PAGE_MAP 18u
#define SYS_PAGE_UNMAP 19u
#define SYS_PAGE_RELEASE 20u
#define SYS_PAGE_QUERY 21u
#define SYS_PAGE_TRANSFER 22u
#define SYS_PAGE_RECEIVE 23u
#define RESOURCE_READ 1u
#define RESOURCE_WRITE 2u
#define RESOURCE_MAP 4u
#define RESOURCE_RELEASE 8u
#define RESOURCE_TRANSFER 16u
#define RESOURCE_QUERY 32u
#define RESOURCE_ALL 63u
#define RESOURCE_WINDOW 0x09000000u
#define RESOURCE_WINDOW_END 0x09010000u
/* Query SYS_RESOURCE_LIMITS for the running kernel's quotas. These constants
 * describe this SDK's reference kernel, not authority granted to a caller. */
#define LIBERUMS_RESOURCE_DOMAIN_LIMIT 4u
#define LIBERUMS_RESOURCE_BINDING_LIMIT 4u
#define RES_EPERM (-1)
#define RES_ENOENT (-2)
#define RES_ESRCH (-3)
#define RES_EAGAIN (-11)
#define RES_ENOMEM (-12)
#define RES_EBUSY (-16)
#define RES_EINVAL (-22)
#define RES_ENOSPC (-28)
#define RES_EOVERFLOW (-75)
#define RES_ESTALE (-116)
#define RES_ENOEXEC (-8)
#define RES_EPROTONOSUPPORT (-93)
struct resource_handle { uint32_t index, generation; };
#endif
