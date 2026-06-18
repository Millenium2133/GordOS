// Exercises the fd-based file syscalls: write a multi-line test file,
// then open it and read it back in small (16-byte) chunks via an fd,
// which forces the kernel to resume from its cached cluster position
// across many reads (and across cluster boundaries for a >cluster file).

#define SYS_WRITE     0
#define SYS_EXIT      1
#define SYS_WRITEFILE 6
#define SYS_OPEN      11
#define SYS_CLOSE     12
#define SYS_READ_FD   13

static inline int syscall3(int n, int a, int b, int c)
{
    int r;
    __asm__ volatile("int $0x80" : "=a"(r)
                     : "a"(n), "b"(a), "c"(b), "d"(c) : "memory");
    return r;
}
static int slen(const char* s){int n=0;while(s[n])n++;return n;}
static void puts2(const char* s){syscall3(SYS_WRITE,(int)s,slen(s),0);}

void _start(void)
{
    // Build a ~1KB file: a start marker, 60 identical lines, an end
    // marker. At 512 bytes/cluster this spans 3 clusters.
    static char file[2048];
    int len = 0;
    const char* head = "FDSTART\n";
    for (int i = 0; head[i]; i++) file[len++] = head[i];
    for (int line = 0; line < 60; line++)
    {
        const char* row = "0123456789ABCDEF\n";
        for (int i = 0; row[i]; i++) file[len++] = row[i];
    }
    const char* tail = "FDEND\n";
    for (int i = 0; tail[i]; i++) file[len++] = tail[i];

    if (syscall3(SYS_WRITEFILE, (int)"FDTEST.TXT", (int)file, len) != 0)
    {
        puts2("fdcat: write failed\n");
        syscall3(SYS_EXIT, 1, 0, 0);
    }

    int fd = syscall3(SYS_OPEN, (int)"FDTEST.TXT", 0, 0);
    if (fd < 0)
    {
        puts2("fdcat: open failed\n");
        syscall3(SYS_EXIT, 1, 0, 0);
    }

    puts2("fdcat: reading back in 16-byte chunks:\n");

    // Read in small chunks until EOF; echo each chunk straight through.
    char chunk[16];
    int total = 0;
    for (;;)
    {
        int n = syscall3(SYS_READ_FD, fd, (int)chunk, sizeof(chunk));
        if (n <= 0)
            break;
        syscall3(SYS_WRITE, (int)chunk, n, 0);
        total += n;
    }

    syscall3(SYS_CLOSE, fd, 0, 0);

    // Report how many bytes came back (should equal what we wrote: 1034)
    char msg[] = "fdcat: read 0000 bytes total\n";
    int t = total;
    msg[12] = '0' + (t / 1000) % 10;
    msg[13] = '0' + (t / 100) % 10;
    msg[14] = '0' + (t / 10) % 10;
    msg[15] = '0' + t % 10;
    puts2(msg);

    syscall3(SYS_EXIT, 0, 0, 0);
    for (;;) {}
}
