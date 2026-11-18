/* SPDX-License-Identifier: GPL-3.0-or-later */
OUTPUT_FORMAT(elf32-i386)
ENTRY(_start)
PHDRS {
    code PT_LOAD FLAGS(5);
    constants PT_LOAD FLAGS(4);
    state PT_LOAD FLAGS(6);
    abi PT_NOTE FLAGS(4);
}
SECTIONS {
    . = 0x08000000;
    .text : { *(.text.entry) *(.text .text.*) } :code
    . = ALIGN(4096);
    .rodata : { *(.rodata .rodata.*) } :constants
    .note.liberums : { KEEP(*(.note.liberums)) } :constants :abi
    . = ALIGN(4096);
    .data : { *(.data .data.*) } :state
    .bss (NOLOAD) : { *(.bss .bss.*) *(COMMON) } :state
    ASSERT(. <= 0x08040000, "Liberums image window exceeded")
    ASSERT(SIZEOF(.note.liberums) == 36, "invalid Liberums ABI note size")
    /DISCARD/ : { *(.comment) *(.note.GNU-stack) *(.eh_frame*) }
}
