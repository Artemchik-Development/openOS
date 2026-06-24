ENTRY(_start)

SECTIONS
{
    /* The conventional loading address for x86 kernels */
    . = 1M;

    /* First put the multiboot header, then the text segment */
    .text : ALIGN(4K)
    {
        *(.multiboot)
        *(.text)
    }

    /* Read-only data (constants, strings) */
    .rodata : ALIGN(4K)
    {
        *(.rodata)
    }

    /* Initialized read-write data */
    .data : ALIGN(4K)
    {
        *(.data)
    }

    /* Uninitialized read-write data and stack */
    .bss : ALIGN(4K)
    {
        *(COMMON)
        *(.bss)
    }
}
