// ush — a tiny user-space shell for GordOS, running entirely in ring 3.
// It reads a line from stdin, and for each command forks a child that
// execs the named ELF, then waits for it. Output can be redirected to a
// file with `>`, e.g.  HELLO.ELF > OUT.TXT  (the child opens the file and
// dup2's it onto stdout before exec, so the program's writes land there).
//
// Run it from the kernel shell with:  exec USH.ELF   — and `exit` to
// return. Commands are run by ELF filename; programs that read keyboard
// input can't run here yet (the kernel keeps one foreground process), and
// there's no argument passing — both are noted as future work.

#define SYS_WRITE 0
#define SYS_EXIT  1
#define SYS_READ  3
#define SYS_FORK  7
#define SYS_EXEC  8
#define SYS_WAIT  9
#define SYS_OPEN  11
#define SYS_CLOSE 12
#define SYS_DUP2  15
#define O_WRITE   1

static inline int sc(int n, int a, int b, int c)
{
    int r;
    __asm__ volatile("int $0x80" : "=a"(r)
                     : "a"(n), "b"(a), "c"(b), "d"(c) : "memory");
    return r;
}

static int slen(const char* s){ int n = 0; while (s[n]) n++; return n; }
static void puts2(const char* s){ sc(SYS_WRITE, (int)s, slen(s), 0); }

static int streq(const char* a, const char* b)
{
    int i = 0;
    while (a[i] && a[i] == b[i]) i++;
    return a[i] == b[i];
}

// Read one line from stdin into buf (NUL-terminated). Handles backspace.
static void read_line(char* buf, int max)
{
    int len = 0;
    for (;;)
    {
        char c;
        if (sc(SYS_READ, (int)&c, 1, 0) <= 0)
            continue;
        if (c == '\n')      { buf[len] = '\0'; return; }
        if (c == '\b')      { if (len > 0) len--; continue; }
        if (len < max - 1)  buf[len++] = c;
    }
}

// Split a line into a command and an optional `> outfile`. Both out
// buffers are NUL-terminated; outfile is empty if there's no redirect.
static void parse(const char* line, char* cmd, char* outfile)
{
    int i = 0, j;

    while (line[i] == ' ') i++;

    j = 0;
    while (line[i] && line[i] != ' ' && line[i] != '>')
        cmd[j++] = line[i++];
    cmd[j] = '\0';

    outfile[0] = '\0';
    while (line[i] == ' ') i++;
    if (line[i] == '>')
    {
        i++;
        while (line[i] == ' ') i++;
        j = 0;
        while (line[i] && line[i] != ' ')
            outfile[j++] = line[i++];
        outfile[j] = '\0';
    }
}

void _start(void)
{
    puts2("ush - user-space shell. Type a program name (e.g. HELLO.ELF),\n");
    puts2("     optionally 'PROG > FILE.TXT', or 'exit'.\n");

    char line[128], cmd[64], outfile[64];

    for (;;)
    {
        puts2("ush$ ");
        read_line(line, sizeof(line));
        parse(line, cmd, outfile);

        if (cmd[0] == '\0')
            continue;
        if (streq(cmd, "exit"))
            break;
        if (streq(cmd, "help"))
        {
            puts2("Run an ELF by name. '>' redirects output to a file. 'exit' quits.\n");
            continue;
        }

        int pid = sc(SYS_FORK, 0, 0, 0);
        if (pid == 0)
        {
            // Child: set up redirection, then become the program
            if (outfile[0])
            {
                int fd = sc(SYS_OPEN, (int)outfile, O_WRITE, 0);
                if (fd < 0)
                {
                    puts2("ush: cannot open ");
                    puts2(outfile);
                    puts2("\n");
                    sc(SYS_EXIT, 1, 0, 0);
                }
                sc(SYS_DUP2, fd, 1, 0);
                sc(SYS_CLOSE, fd, 0, 0);
            }
            sc(SYS_EXEC, (int)cmd, 0, 0);
            // exec only returns on failure
            puts2("ush: command not found: ");
            puts2(cmd);
            puts2("\n");
            sc(SYS_EXIT, 127, 0, 0);
        }
        else if (pid > 0)
        {
            int status = 0;
            sc(SYS_WAIT, (int)&status, 0, 0);
        }
        else
        {
            puts2("ush: fork failed\n");
        }
    }

    puts2("ush: bye\n");
    sc(SYS_EXIT, 0, 0, 0);
    for (;;) {}
}
