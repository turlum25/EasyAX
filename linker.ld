ENTRY(_start)
SECTIONS
{
    . = 0x100000; /* load kernel at 1MB */

    .text : ALIGN(4K)
    {
        *(.multiboot) /* multiboot header goes first! */
        *(.text)
    }

    .rodata : ALIGN(4K) { *(.rodata) }
    .data   : ALIGN(4K) { *(.data) }
    .bss    : ALIGN(4K) { *(.bss) }

    _kernel_end = .;
}