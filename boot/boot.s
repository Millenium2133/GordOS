.set ALIGN,    1<<0
.set MEMINFO,  1<<1
.set FLAGS,    ALIGN | MEMINFO
.set MAGIC,    0x1BADB002
.set CHECKSUM, -(MAGIC + FLAGS)

.set KERNEL_VIRTUAL_BASE, 0xC0000000

.section .boot, "ax"
.align 4
.long MAGIC
.long FLAGS
.long CHECKSUM

.global _start
.type _start, @function
_start:
    # Fill boot_page_table to identity-map the first 4MB
    movl $(boot_page_table - KERNEL_VIRTUAL_BASE), %edi
    movl $0x3, %eax
    movl $1024, %ecx
1:
    movl %eax, (%edi)
    addl $0x1000, %eax
    addl $4, %edi
    loop 1b

    # Install page table at entry 0 (identity) and entry 768 (higher half)
    movl $(boot_page_directory - KERNEL_VIRTUAL_BASE), %edi
    movl $(boot_page_table - KERNEL_VIRTUAL_BASE), %eax
    orl  $0x3, %eax
    movl %eax, 0(%edi)
    movl %eax, 768*4(%edi)

    # Load CR3 and enable paging
    movl $(boot_page_directory - KERNEL_VIRTUAL_BASE), %ecx
    movl %ecx, %cr3
    movl %cr0, %ecx
    orl  $0x80000000, %ecx
    movl %ecx, %cr0

    # Jump into higher half
    leal higher_half_start, %ecx
    jmp *%ecx
.size _start, . - _start

.section .bss
.align 4096
.global boot_page_directory
boot_page_directory:
    .skip 4096
.global boot_page_table
boot_page_table:
    .skip 4096

.align 16
stack_bottom:
    .skip 65536
stack_top:

.section .text
.global higher_half_start
.type higher_half_start, @function
higher_half_start:
    movl $stack_top, %esp
    push %ebx
    push %eax
    call kernel_main
    cli
1:  hlt
    jmp 1b
.size higher_half_start, . - higher_half_start