// cat2 — copy stdin to stdout until EOF.
// Used as the consumer in pipe tests: ECHO.ELF foo | CAT2.ELF
//
// Build:   make user
// Install: make disk  (copied as CAT2.ELF)

#define SYS_WRITE 0
#define SYS_EXIT  1
#define SYS_READ  3

static inline int sc(int n, int a, int b, int c)
{
    int r;
    __asm__ volatile("int $0x80"
                     : "=a"(r)
                     : "a"(n), "b"(a), "c"(b), "d"(c)
                     : "memory");
    return r;
}

void _start(void)
{
    char buf[256];
    int n;
    while ((n = sc(SYS_READ, (int)buf, sizeof(buf), 0)) > 0)
        sc(SYS_WRITE, (int)buf, n, 0);
    sc(SYS_EXIT, 0, 0, 0);
    for (;;) {}
}
