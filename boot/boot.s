.set ALIGN,    1<<0
.set MEMINFO,  1<<1
.set FLAGS,    ALIGN | MEMINFO
.set MAGIC,    0x1BADB002
.set CHECKSUM, -(MAGIC + FLAGS)

.set KERNEL_VIRTUAL_BASE, 0xC0000000

.section .multiboot
.align 4
.long MAGIC
.long FLAGS
.long CHECKSUM

# Boot page tables live in .bss (zeroed by GRUB before _start runs).
# .bss is linked at higher-half VMA, so we subtract KERNEL_VIRTUAL_BASE
# to get the physical address while paging is off.
.section .bss
.align 4096
boot_page_directory:
    .skip 4096
boot_page_table:
    .skip 4096

.align 16
stack_bottom:
    .skip 16384
stack_top:

# Bootstrap: low VMA=LMA, runs with paging off.
.section .boot, "ax"
.global _start
.type _start, @function
_start:
    # Fill boot_page_table to identity-map the first 4MB.
    movl $(boot_page_table - KERNEL_VIRTUAL_BASE), %edi
    movl $0x3, %eax              # phys 0x000 | PRESENT | RW
    movl $1024, %ecx
1:
    movl %eax, (%edi)
    addl $0x1000, %eax
    addl $4, %edi
    loop 1b

    # Install the page table at directory entry 0 (identity at 0x00000000)
    # and entry 768 (higher half at 0xC0000000). 0xC0000000 / 4MB = 768.
    movl $(boot_page_directory - KERNEL_VIRTUAL_BASE), %edi
    movl $(boot_page_table - KERNEL_VIRTUAL_BASE), %eax
    orl  $0x3, %eax
    movl %eax, 0(%edi)
    movl %eax, 768*4(%edi)

    # Load CR3, then set CR0.PG.
    movl $(boot_page_directory - KERNEL_VIRTUAL_BASE), %ecx
    movl %ecx, %cr3
    movl %cr0, %ecx
    orl  $0x80000000, %ecx
    movl %ecx, %cr0

    # Indirect jump into higher half. After this, EIP is virtual.
    leal higher_half_start, %ecx
    jmp *%ecx
.size _start, . - _start

# Higher-half entry, runs with paging on. Linked at 0xC02xxxxx.
.section .text
.global higher_half_start
.type higher_half_start, @function
higher_half_start:
    movl $stack_top, %esp        # higher-half stack
    push %ebx                    # multiboot info pointer
    push %eax                    # multiboot magic
    call kernel_main
    cli
1:  hlt
    jmp 1b
.size higher_half_start, . - higher_half_start
