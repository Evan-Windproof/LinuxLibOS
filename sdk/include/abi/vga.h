/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef LIBERUMS_ABI_VGA_H
#define LIBERUMS_ABI_VGA_H
#include <abi/syscall.h>
/* Optional, scalar-only extension to ABI 1.2. Probe INFO: older kernels return
 * ENOSYS (-38). No existing version, feature bit or syscall changes meaning. */
#define SYS_VGA_INFO 32u
#define SYS_VGA_ACQUIRE 33u
#define SYS_VGA_MAP 34u
#define SYS_VGA_RELEASE 35u
#define SYS_VGA_CURSOR 36u /* EBX=index, ECX=generation, EDX=cell 0..1999. */
#define VGA_DEVICE_TEXT 1u
#define VGA_DEVICE_HANDLE 0x00010001u /* Separate namespace from ordinary RAM. */
#define VGA_USER_ADDRESS 0x09400000u
#define VGA_COLUMNS 80u
#define VGA_ROWS 25u
#define VGA_MAPPING_BYTES 4096u
/* INFO: EBX=device; EAX=columns | rows<<16; EDX=mapping bytes.
 * ACQUIRE: EBX=device, ECX=READ[|WRITE]; EAX=index, EDX=generation.
 * MAP: EBX=index, ECX=generation, EDX=VA, ESI=READ[|WRITE].
 * RELEASE: EBX=index, ECX=generation; destroys the handle and its binding.
 * Errors are negative RES_*; output EDX on INFO/ACQUIRE error is zero.
 * Discoverability is not authority. Only a trusted boot grant allows ACQUIRE.
 * One exclusive handle and at most one binding. No port I/O or RAM transfer.
 * The 4000-byte screen sits in one 4096-byte device page; its final 96 bytes
 * are VGA aperture padding, accessible at page granularity, not unrelated RAM. */
#endif
