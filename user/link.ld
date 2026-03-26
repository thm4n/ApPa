/*
 * link.ld — Linker script for ApPa user-space ELF programs
 *
 * Links the program at virtual address 0x08048000 (traditional i386
 * default), producing a statically-linked ET_EXEC ELF32 binary.
 *
 * The ELF loader in the kernel reads PT_LOAD program headers and maps
 * each segment into the process's private page directory.
 */

ENTRY(_start)

SECTIONS
{
    /* Base virtual address for user programs */
    . = 0x08048000;

    .text : ALIGN(4096)
    {
        *(.text*)
    }

    .rodata : ALIGN(4)
    {
        *(.rodata*)
    }

    .data : ALIGN(4096)
    {
        *(.data*)
    }

    .bss : ALIGN(4)
    {
        *(.bss*)
        *(COMMON)
    }

    /* Discard unwanted sections */
    /DISCARD/ :
    {
        *(.comment)
        *(.note*)
        *(.eh_frame*)
    }
}
