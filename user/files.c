// File syscall demo for GordOS: writes a file to disk via
// sys_writefile, reads it back via sys_readfile, and prints it.
//
// Build:    make user
// Install:  make disk   (copied as FILES.ELF)
// Run:      exec FILES.ELF

#define SYS_WRITE     0
#define SYS_EXIT      1
#define SYS_GETPID    2
#define SYS_READ      3
#define SYS_SLEEP     4
#define SYS_READFILE  5
#define SYS_WRITEFILE 6

static inline int syscall3(int num, int arg1, int arg2, int arg3)
{
    int ret;
    __asm__ volatile("int $0x80"
                     : "=a"(ret)
                     : "a"(num), "b"(arg1), "c"(arg2), "d"(arg3)
                     : "memory");
    return ret;
}

static int str_len(const char* s)
{
    int n = 0;
    while (s[n])
        n++;
    return n;
}

static void write_str(const char* s)
{
    syscall3(SYS_WRITE, (int)s, str_len(s), 0);
}

void _start(void)
{
    const char* path = "USRTEST.TXT";
    const char* content = "Written from ring 3 via sys_writefile!\n";

    write_str("Writing USRTEST.TXT...\n");
    if (syscall3(SYS_WRITEFILE, (int)path, (int)content, str_len(content)) != 0)
    {
        write_str("write failed!\n");
        syscall3(SYS_EXIT, 1, 0, 0);
    }

    write_str("Reading it back: ");
    char buf[256];
    int n = syscall3(SYS_READFILE, (int)path, (int)buf, sizeof(buf));
    if (n < 0)
    {
        write_str("read failed!\n");
        syscall3(SYS_EXIT, 1, 0, 0);
    }
    syscall3(SYS_WRITE, (int)buf, n, 0);

    syscall3(SYS_EXIT, 0, 0, 0);
    for (;;) {}
}
