# crt0.s
# GordOS userland C runtime startup stub (Phase 1.3)
#
# process_bootstrap/paging_build_user_stack already leave the standard
# i686 _start frame on the stack when a program is entered:
#   [esp]       = fake return address (0)
#   [esp+4]     = argc
#   [esp+8]     = argv
#   [esp+12]    = envp
#
# So all _start has to do is unpack that, call main(argc, argv), and hand
# main's return value to sys_exit. No libc call for exit yet, the syscall
# is issued directly here since crt0 has to be self-contained (no shared libc
# exists yet, see README Phase 1 groundwork notes).

.section .text
.global _start
.extern main

_start:
    mov 4(%esp), %eax   # argc
    mov 8(%esp), %ecx   # argv

    push %ecx           # argv
    push %eax           # argc
    call main
    add $8, %esp        # pop main's args back off

    # main's return value (in %eax) becomes the process exit code
    mov %eax, %ebx      # sys_exit's arg1
    mov $1, %eax        # SYS_EXIT
    int $0x80

    # sys_exit does not return, but just in case
.hang:
    hlt
    jmp .hang

.size _start, . - _start

.section .note.GNU-stack, "", @progbits