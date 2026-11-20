/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Independent Linux i386 image. The loop writes its computed digit to data. */
.section .text
.global _start
.type _start, @function
_start:
    movl $4, %ecx
    movl $0x0a30, %edx
1:
    addl $2, %edx
    subl $1, %ecx
    jnz 1b
    pushl %edx
    popl %ebp
    movl %ebp, %esi
    movl %esi, digit

    movl $4, %eax
    movl $1, %ebx
    movl $message, %ecx
    movl $message_size, %edx
    int $0x80
    cmpl $message_size, %eax
    jnz 2f
    movl $37, %ebx
    jmp 3f
2:
    movl $99, %ebx
3:
    movl $1, %eax
    int $0x80
.size _start, . - _start

.section .data
message:
    .ascii "Loop result: "
digit:
    .long 0
.set message_size, digit - message + 2

.section .note.GNU-stack, "", @progbits
