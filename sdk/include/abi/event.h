/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef LIBERUMS_ABI_EVENT_H
#define LIBERUMS_ABI_EVENT_H
#define SYS_REVOKE_POLL 24u
#define SYS_REVOKE_INFO 25u
#define SYS_REVOKE_ACK 26u
#define REVOKE_EVENTS_PER_DOMAIN 2u
#define REVOKE_REASON_PRESSURE 1u
#define REVOKE_REASON_TEST 2u
#define REVOKE_INFO_INDEX 0u
#define REVOKE_INFO_GENERATION 1u
#define REVOKE_INFO_REASON 2u
#define REVOKE_INFO_DEADLINE 3u
/* POLL: EAX=status, EDX=request ID (oldest live event; non-consuming).
 * INFO: EBX=request ID, ECX=field; EAX=status, EDX=32-bit value.
 * ACK: EBX=index, ECX=generation, EDX=request ID; EAX=status.
 * All fields are scalars. No kernel dereference of user pointers. */
#endif
