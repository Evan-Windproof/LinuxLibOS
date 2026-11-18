/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef LIBERUMS_ABI_KEYBOARD_H
#define LIBERUMS_ABI_KEYBOARD_H
#include <abi/syscall.h>
/* Optional scalar extension, probe INFO; ABI 1.2 and old features unchanged. */
#define SYS_KBD_INFO 40u
#define SYS_KBD_ACQUIRE 41u
#define SYS_KBD_POLL 42u
#define SYS_KBD_RELEASE 43u
#define SYS_KBD_STATS 44u
#define KBD_DEVICE_PS2 1u
#define KBD_HANDLE 0x00010002u
#define KBD_QUEUE_CAPACITY 64u
#define KBD_SCANSET1 1u
/* INFO: EBX=device, EAX=capacity, EDX=scan format (translated set 1).
 * ACQUIRE: EBX=device, EAX=handle index, EDX=generation; needs trusted grant.
 * POLL: EBX=index, ECX=generation; EAX=0, EDX=raw byte, or EAGAIN when empty.
 * On EOVERFLOW, EDX=discarded byte count: whole partial batch is discarded;
 * decoder/editor must reset. Counters saturate, never wrap.
 * STATS: same handle; EAX=current count, EDX=lifetime lost bytes.
 * RELEASE: same handle; EAX=0; clear all queued input, invalidate handle.
 * Error EDX=0 except POLL overflow. No pointer, keymap or arbitrary port API. */
#endif
