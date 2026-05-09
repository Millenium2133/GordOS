ENTRY(_start)

KERNEL_VIRTUAL_BASE = 0xC0000000;

SECTIONS
{
    . = 2M;

    .boot : {
        KEEP(*(.boot))
    }

    . = ALIGN(4K);
    . += KERNEL_VIRTUAL_BASE;

    .text : AT(ADDR(.text) - KERNEL_VIRTUAL_BASE)
    {
        *(.text)
    }

    . = ALIGN(4K);
    .rodata : AT(ADDR(.rodata) - KERNEL_VIRTUAL_BASE)
    {
        *(.rodata)
    }

    . = ALIGN(4K);
    .data : AT(ADDR(.data) - KERNEL_VIRTUAL_BASE)
    {
        *(.data)
    }

    . = ALIGN(4K);
    .bss : AT(ADDR(.bss) - KERNEL_VIRTUAL_BASE)
    {
        *(COMMON)
        *(.bss)
    }
}