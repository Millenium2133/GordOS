.section .text
.global scheduler_asm_switch
.type scheduler_asm_switch, @function

# void scheduler_asm_switch(uint32_t* old_esp, uint32_t new_esp)
# Arguments: 4(%esp) = pointer to save old esp into
#            8(%esp) = new esp to restore
scheduler_asm_switch:
    # Save all callee-saved registers onto the current stack
    push %ebp
    push %edi
    push %esi
    push %ebx

    # Save current esp into *old_esp
    mov 20(%esp), %eax    # old_esp pointer (4 args + 4 pushed regs = 20 bytes)
    mov %esp, (%eax)

    # Load new esp
    mov 24(%esp), %esp    # new_esp (24 bytes from original esp)

    # Restore callee-saved registers from new stack
    pop %ebx
    pop %esi
    pop %edi
    pop %ebp

    ret

.size scheduler_asm_switch, . - scheduler_asm_switch

.section .note.GNU-stack, "", @progbits
