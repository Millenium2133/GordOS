.section .text

# void process_launch(uint32_t entry, uint32_t user_esp, uint32_t* save_esp)
#
# Saves the current kernel context (callee-saved registers + esp) into
# *save_esp, then switches to ring 3 at entry with the given user stack.
# This function "returns" when process_return() is later called with the
# saved esp — execution resumes in the caller as if process_launch had
# returned normally.
.global process_launch
.type process_launch, @function
process_launch:
    push %ebp
    push %edi
    push %esi
    push %ebx

    # After the 4 pushes the arguments live at:
    #   20(%esp) = entry, 24(%esp) = user_esp, 28(%esp) = save_esp
    mov 28(%esp), %eax
    mov %esp, (%eax)         # save kernel context for process_return

    mov 20(%esp), %edx       # user eip
    mov 24(%esp), %ecx       # user esp

    # Load user data segment selectors (0x23 = GDT entry 4, RPL 3).
    # Stack accesses still use ss (kernel) until iret.
    mov $0x23, %ax
    mov %ax, %ds
    mov %ax, %es
    mov %ax, %fs
    mov %ax, %gs

    # Build the iret frame: ss, esp, eflags (IF set), cs, eip
    push $0x23
    push %ecx
    pushf
    pop %eax
    or $0x200, %eax
    push %eax
    push $0x1B               # user code segment (GDT entry 3, RPL 3)
    push %edx
    iret
.size process_launch, . - process_launch

# void process_return(uint32_t saved_esp)
#
# Switches back to a kernel context saved by process_launch. Never
# returns to its caller — execution continues after the original
# process_launch call.
.global process_return
.type process_return, @function
process_return:
    mov 4(%esp), %eax
    mov %eax, %esp
    pop %ebx
    pop %esi
    pop %edi
    pop %ebp
    ret
.size process_return, . - process_return

.section .note.GNU-stack, "", @progbits
