/* Linker script for GordOS user programs.
 * Loaded by the kernel's ELF loader at the fixed user code address. */

ENTRY(_start)

SECTIONS
{
    . = 0x00100000;

    .text   : { *(.text*) }
    .rodata : { *(.rodata*) }

    . = ALIGN(4096);
    .data   : { *(.data*) }
    .bss    : { *(.bss*) *(COMMON) }
}
