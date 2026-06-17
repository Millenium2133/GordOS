.section .text

# void process_bootstrap(void)
#
# First code a new process's kernel context runs. scheduler_asm_switch
# pops the four dummy callee-saved registers from the freshly built
# kernel stack and rets here, leaving an iret frame (eip, cs, eflags,
# user esp, ss) on top of the stack — built by process_start().
# Load the user data segments and drop to ring 3.
.global process_bootstrap
.type process_bootstrap, @function
process_bootstrap:
    mov $0x23, %ax           # user data segment (GDT entry 4, RPL 3)
    mov %ax, %ds
    mov %ax, %es
    mov %ax, %fs
    mov %ax, %gs
    iret
.size process_bootstrap, . - process_bootstrap

.section .note.GNU-stack, "", @progbits
