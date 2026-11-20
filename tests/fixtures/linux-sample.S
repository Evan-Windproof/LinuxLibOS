/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Independent Linux i386 image. EAX=4 is write, EAX=1 is exit. */
.section .text
.global _start
.type _start, @function
_start:
    movl $4, %eax
    movl $1, %ebx
    movl $message, %ecx
    movl $message_size, %edx
    int $0x80

    movl $1, %eax
    movl exit_status, %ebx
    int $0x80
.size _start, . - _start

.section .rodata
message:
    .ascii "Hello from Linux i386\n"
.set message_size, . - message

.section .data
exit_status:
    .long 0

.section .bss
    .balign 16
    .skip 96

.section .note.GNU-stack, "", @progbits
