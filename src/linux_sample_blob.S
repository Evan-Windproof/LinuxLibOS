/* SPDX-License-Identifier: GPL-3.0-or-later */
.section .rodata
.global linux_sample_start, linux_sample_end
linux_sample_start:
.incbin "build/linux-sample.elf"
linux_sample_end:
.section .note.GNU-stack, "", @progbits
