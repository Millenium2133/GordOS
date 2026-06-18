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

# void fork_child_return(void)
#
# Where a forked child resumes. process_fork builds the child's kernel
# stack so scheduler_asm_switch rets here with esp pointing at a saved
# register frame copied from the parent (the int 0x80 frame, with eax
# forced to 0). This is exactly the tail of syscall_common: restore the
# segments and registers and iret back into ring 3, right after the
# fork() the parent called.
.global fork_child_return
.type fork_child_return, @function
fork_child_return:
    pop %eax                 # saved ds
    mov %ax, %ds
    mov %ax, %es
    mov %ax, %fs
    mov %ax, %gs
    popa
    add $8, %esp             # skip int_no, err_code
    iret
.size fork_child_return, . - fork_child_return

.section .note.GNU-stack, "", @progbits
