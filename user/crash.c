// Deliberately faults, to test that the kernel kills a misbehaving
// user process and returns to the shell instead of halting.
//
// Run: exec CRASH.ELF — expect a page fault report, then the prompt.

#define SYS_WRITE 0

static inline int syscall3(int num, int arg1, int arg2, int arg3)
{
    int ret;
    __asm__ volatile("int $0x80"
                     : "=a"(ret)
                     : "a"(num), "b"(arg1), "c"(arg2), "d"(arg3)
                     : "memory");
    return ret;
}

void _start(void)
{
    const char msg[] = "About to dereference a null pointer...\n";
    syscall3(SYS_WRITE, (int)msg, sizeof(msg) - 1, 0);

    volatile int* null_ptr = 0;
    *null_ptr = 42; // page fault: address 0 is never mapped

    // Should never get here
    const char bad[] = "Survived?! That's a bug.\n";
    syscall3(SYS_WRITE, (int)bad, sizeof(bad) - 1, 0);
    for (;;) {}
}
