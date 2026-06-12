// Interactive demo for GordOS: reads a line from the keyboard via
// sys_read and prints it back.
//
// Build:    make user
// Install:  make disk   (copied as ECHO.ELF)
// Run:      exec ECHO.ELF

#define SYS_WRITE  0
#define SYS_EXIT   1
#define SYS_GETPID 2
#define SYS_READ   3
#define SYS_SLEEP  4

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
    while (s[n])
        n++;
    syscall3(SYS_WRITE, (int)s, n, 0);
}

// Read one line (up to max-1 chars), handling backspace.
// The kernel echoes input as it is consumed.
static void read_line(char* buf, int max)
{
    int pos = 0;

    for (;;)
    {
        char c;
        if (syscall3(SYS_READ, (int)&c, 1, 0) != 1)
            continue;

        if (c == '\n')
            break;

        if (c == '\b')
        {
            if (pos > 0)
                pos--;
            continue;
        }

        if (pos < max - 1)
            buf[pos++] = c;
    }

    buf[pos] = '\0';
}

void _start(void)
{
    char line[128];

    write_str("Type a line and press Enter:\n> ");
    read_line(line, sizeof(line));

    write_str("You typed: ");
    write_str(line);
    write_str("\n");

    syscall3(SYS_SLEEP, 500, 0, 0);
    write_str("Bye!\n");

    syscall3(SYS_EXIT, 0, 0, 0);
    for (;;) {}
}
