/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef LINUX_SAMPLE_FILE
#define LINUX_SAMPLE_FILE "build/linux-sample.elf"
#endif
.section .rodata
.global linux_sample_start, linux_sample_end
linux_sample_start:
.incbin LINUX_SAMPLE_FILE
linux_sample_end:
.section .note.GNU-stack, "", @progbits
