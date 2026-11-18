/* SPDX-License-Identifier: GPL-3.0-or-later */
.code32
.section .text.entry,"ax",@progbits
.global _start
.type _start,@function
_start:
    cld
    xorl %ebp,%ebp
    andl $-16,%esp
    call libos_start
    ud2
.size _start,.-_start
.section .note.GNU-stack,"",@progbits
