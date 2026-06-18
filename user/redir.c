// Self-contained test of writable fds + dup2 redirection: redirect
// stdout to a file, write to it, restore stdout, then read the file back
// to prove the bytes landed on disk. Run with `exec REDIR.ELF`.

#define SYS_WRITE   0
#define SYS_EXIT    1
#define SYS_OPEN    11
#define SYS_CLOSE   12
#define SYS_READ_FD 13
#define SYS_DUP2    15
#define O_WRITE     1

static inline int sc(int n, int a, int b, int c)
{
    int r;
    __asm__ volatile("int $0x80" : "=a"(r)
                     : "a"(n), "b"(a), "c"(b), "d"(c) : "memory");
    return r;
}
static int slen(const char* s){ int n = 0; while (s[n]) n++; return n; }
static void puts2(const char* s){ sc(SYS_WRITE, (int)s, slen(s), 0); }

void _start(void)
{
    puts2("redir: redirecting stdout to ROUT.TXT\n");

    int fd = sc(SYS_OPEN, (int)"ROUT.TXT", O_WRITE, 0);
    if (fd < 0)
    {
        puts2("redir: open failed\n");
        sc(SYS_EXIT, 1, 0, 0);
    }

    sc(SYS_DUP2, fd, 1, 0);          // stdout now goes to the file
    puts2("captured line one\n");    // ...so these land in ROUT.TXT
    puts2("captured line two\n");
    sc(SYS_CLOSE, fd, 0, 0);         // drop the extra reference
    sc(SYS_DUP2, 2, 1, 0);           // restore stdout from stderr; flushes file

    puts2("redir: reading ROUT.TXT back:\n");
    int rf = sc(SYS_OPEN, (int)"ROUT.TXT", 0, 0);
    if (rf < 0)
    {
        puts2("redir: reopen failed\n");
        sc(SYS_EXIT, 1, 0, 0);
    }
    char buf[32];
    int n;
    while ((n = sc(SYS_READ_FD, rf, (int)buf, sizeof(buf))) > 0)
        sc(SYS_WRITE, (int)buf, n, 0);
    sc(SYS_CLOSE, rf, 0, 0);

    puts2("redir: ok\n");
    sc(SYS_EXIT, 0, 0, 0);
    for (;;) {}
}
