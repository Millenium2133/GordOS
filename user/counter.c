// Background demo for GordOS: counts to 5, pausing between each number,
// then exits. Run it with `bg COUNTER.ELF` and keep using the shell
// while it prints — the scheduler time-slices both.

#define SYS_WRITE 0
#define SYS_EXIT  1
#define SYS_SLEEP 4

static inline int syscall3(int num, int arg1, int arg2, int arg3)
{
    int ret;
    __asm__ volatile("int $0x80"
                     : "=a"(ret)
                     : "a"(num), "b"(arg1), "c"(arg2), "d"(arg3)
                     : "memory");
    return ret;
}

static void write_str(const char* s)
{
    int n = 0;
    while (s[n]) n++;
    syscall3(SYS_WRITE, (int)s, n, 0);
}

void _start(void)
{
    for (int i = 1; i <= 5; i++)
    {
        char msg[] = "[counter] tick 0\n";
        msg[15] = (char)('0' + i);
        write_str(msg);
        syscall3(SYS_SLEEP, 800, 0, 0);
    }
    write_str("[counter] done counting\n");
    syscall3(SYS_EXIT, 0, 0, 0);
    for (;;) {}
}
