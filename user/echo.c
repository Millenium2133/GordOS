// echo — print arguments to stdout, space-separated, with a newline.
// argv[1..argc-1] are written; nothing is printed when argc < 2.
//
// Build:   make user
// Install: make disk  (copied as ECHO.ELF)
// Run:     exec ECHO.ELF hello world

#define SYS_WRITE 0
#define SYS_EXIT  1

static inline int sc(int n, int a, int b, int c)
{
    int r;
    __asm__ volatile("int $0x80"
                     : "=a"(r)
                     : "a"(n), "b"(a), "c"(b), "d"(c)
                     : "memory");
    return r;
}

static int slen(const char* s) { int n = 0; while (s[n]) n++; return n; }

static void write_str(const char* s)
{
    sc(SYS_WRITE, (int)s, slen(s), 0);
}

void _start(int argc, char** argv)
{
    for (int i = 1; i < argc; i++)
    {
        if (i > 1) write_str(" ");
        write_str(argv[i]);
    }
    write_str("\n");
    sc(SYS_EXIT, 0, 0, 0);
    for (;;) {}
}
