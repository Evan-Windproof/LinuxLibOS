/* SPDX-License-Identifier: GPL-3.0-or-later */
.code32
.section .text
.global app_call
.type app_call,@function
app_call:
    pushl %ebp
    pushl %ebx
    pushl %esi
    pushl %edi
    movl %esp, app_saved_stack
    movl 20(%esp), %eax
    subl $12, %esp
    call *%eax
    addl $12, %esp
.Lreturn:
    popl %edi
    popl %esi
    popl %ebx
    popl %ebp
    ret
.size app_call,.-app_call

/* A controlled application failure returns to app_run's original C frame.
   This is used only during app_call, in the same domain and on the same stack. */
.global app_unwind
.type app_unwind,@function
app_unwind:
    movl 4(%esp), %eax
    movl app_saved_stack, %esp
    cld
    jmp .Lreturn
.size app_unwind,.-app_unwind

.section .bss
.balign 4
app_saved_stack: .long 0
.section .note.GNU-stack,"",@progbits
