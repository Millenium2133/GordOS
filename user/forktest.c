// Exercises fork()/exec()/wait(): the parent forks a child, the child
// runs a second program via exec(), and the parent waits for it and
// reports the exit code.
//
// Build into the disk image with `make disk`, then run `exec FORKTEST.ELF`.

#define SYS_WRITE   0
#define SYS_EXIT    1
#define SYS_GETPID  2
#define SYS_FORK    7
#define SYS_EXEC    8
#define SYS_WAIT    9

static inline int syscall3(int num, int a1, int a2, int a3)
{
    int ret;
    __asm__ volatile("int $0x80"
                     : "=a"(ret)
                     : "a"(num), "b"(a1), "c"(a2), "d"(a3)
                     : "memory");
    return ret;
}

static int str_len(const char* s)
{
    int n = 0;
    while (s[n]) n++;
    return n;
}

static void write_str(const char* s)
{
    syscall3(SYS_WRITE, (int)s, str_len(s), 0);
}

// Print "label" followed by a small non-negative number and a newline.
static void write_num(const char* label, int n)
{
    char buf[16];
    int i = 0;
    if (n == 0)
        buf[i++] = '0';
    else
    {
        char tmp[12];
        int t = 0;
        while (n > 0 && t < 12) { tmp[t++] = (char)('0' + (n % 10)); n /= 10; }
        while (t > 0) buf[i++] = tmp[--t];
    }
    buf[i++] = '\n';
    write_str(label);
    syscall3(SYS_WRITE, (int)buf, i, 0);
}

void _start(void)
{
    write_str("forktest: start\n");

    int pid = syscall3(SYS_FORK, 0, 0, 0);

    if (pid == 0)
    {
        // Child: replace ourselves with HELLO.ELF via exec
        write_str("forktest: child execing HELLO.ELF\n");
        syscall3(SYS_EXEC, (int)"HELLO.ELF", 0, 0);
        // exec only returns on failure
        write_str("forktest: exec failed\n");
        syscall3(SYS_EXIT, 1, 0, 0);
    }
    else if (pid > 0)
    {
        // Parent: wait for the child and report its exit code
        int status = -1;
        int child = syscall3(SYS_WAIT, (int)&status, 0, 0);
        write_num("forktest: reaped child pid ", child);
        write_num("forktest: child exit code ", status);
        write_str("forktest: parent done\n");
        syscall3(SYS_EXIT, 0, 0, 0);
    }
    else
    {
        write_str("forktest: fork failed\n");
        syscall3(SYS_EXIT, 1, 0, 0);
    }

    for (;;) {}
}
