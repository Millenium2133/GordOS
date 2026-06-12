// Minimal freestanding user program for GordOS.
//
// Build:    make user           (produces user/hello.elf)
// Install:  mcopy -i disk.img user/hello.elf ::HELLO.ELF
//           (or just `make disk`, which copies it automatically)
// Run:      exec HELLO.ELF      (from the GordOS shell)

#define SYS_WRITE  0
#define SYS_EXIT   1
#define SYS_GETPID 2

static inline int syscall3(int num, int arg1, int arg2, int arg3)
{
    int ret;
    __asm__ volatile("int $0x80"
                     : "=a"(ret)
                     : "a"(num), "b"(arg1), "c"(arg2), "d"(arg3)
                     : "memory");
    return ret;
}

static void write(const char* s, int len)
{
    syscall3(SYS_WRITE, (int)s, len, 0);
}

static int str_len(const char* s)
{
    int n = 0;
    while (s[n])
        n++;
    return n;
}

void _start(void)
{
    const char* msg = "Hello from ring 3!\n";
    write(msg, str_len(msg));

    int pid = syscall3(SYS_GETPID, 0, 0, 0);
    char pidmsg[] = "My PID is 0\n";
    pidmsg[10] = (char)('0' + (pid % 10));
    write(pidmsg, sizeof(pidmsg) - 1);

    syscall3(SYS_EXIT, 0, 0, 0);

    // sys_exit does not return, but the compiler doesn't know that
    for (;;) {}
}
