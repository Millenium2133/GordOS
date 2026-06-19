// ush — user-space shell for GordOS.
//
// Features:
//  - Argument passing to programs (argv is forwarded via SYS_EXEC)
//  - Keyboard focus handoff (child owns keyboard while running)
//  - Output redirection: PROG [ARGS] > FILE
//  - Pipes: PROG1 [ARGS] | PROG2 [ARGS]
//
// Run from the kernel shell with:  exec USH.ELF   and `exit` to return.

#define SYS_WRITE           0
#define SYS_EXIT            1
#define SYS_READ            3
#define SYS_FORK            7
#define SYS_EXEC            8
#define SYS_WAIT            9
#define SYS_WAITPID         10
#define SYS_OPEN            11
#define SYS_CLOSE           12
#define SYS_DUP2            15
#define SYS_GIVE_FOREGROUND 16
#define SYS_PIPE            17
#define O_WRITE             1

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
static void puts2(const char* s) { sc(SYS_WRITE, (int)s, slen(s), 0); }

static int streq(const char* a, const char* b)
{
    int i = 0;
    while (a[i] && a[i] == b[i]) i++;
    return a[i] == b[i];
}

static void read_line(char* buf, int max)
{
    int len = 0;
    for (;;)
    {
        char c;
        if (sc(SYS_READ, (int)&c, 1, 0) <= 0)
            continue;
        if (c == '\n')     { buf[len] = '\0'; return; }
        if (c == '\b')     { if (len > 0) len--; continue; }
        if (len < max - 1) buf[len++] = c;
    }
}

// Tokenise a portion of a line [line, line+len) into up to max tokens.
// Returns the number of tokens found. Also extracts an optional `> FILE`
// at the end into outfile (empty string if not present).
// Each tok[i] points into a separate buffer; all strings are NUL-terminated.
#define MAX_TOKS 9
static int tokenise(const char* line, int len,
                    char toks[][128], int max_toks, char* outfile)
{
    int n = 0, i = 0;
    outfile[0] = '\0';

    while (i < len && n < max_toks)
    {
        while (i < len && line[i] == ' ') i++;
        if (i >= len) break;

        if (line[i] == '>')
        {
            i++;
            while (i < len && line[i] == ' ') i++;
            int j = 0;
            while (i < len && line[i] != ' ') outfile[j++] = line[i++];
            outfile[j] = '\0';
            break;
        }

        int j = 0;
        while (i < len && line[i] != ' ' && line[i] != '>' && j < 127)
            toks[n][j++] = line[i++];
        toks[n][j] = '\0';
        n++;
    }
    return n;
}

// Run a single command (no pipe).  toks[0] is the program, toks[1..n-1]
// are its arguments.  outfile may be non-empty for redirection.
// Gives keyboard focus to the child and waits for it to exit.
static void run_simple(char toks[][128], int ntoks, const char* outfile)
{
    if (ntoks == 0) return;

    // Build a NULL-terminated argv array on the stack
    const char* argv[MAX_TOKS + 1];
    for (int i = 0; i < ntoks && i < MAX_TOKS; i++)
        argv[i] = toks[i];
    argv[ntoks < MAX_TOKS ? ntoks : MAX_TOKS] = 0;

    int pid = sc(SYS_FORK, 0, 0, 0);
    if (pid == 0)
    {
        // Child: set up optional output redirect, then exec
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
        sc(SYS_EXEC, (int)toks[0], (int)argv, 0);
        puts2("ush: not found: ");
        puts2(toks[0]);
        puts2("\n");
        sc(SYS_EXIT, 127, 0, 0);
    }
    else if (pid > 0)
    {
        sc(SYS_GIVE_FOREGROUND, pid, 0, 0);
        int status = 0;
        sc(SYS_WAITPID, pid, (int)&status, 0);
    }
    else
    {
        puts2("ush: fork failed\n");
    }
}

// Run a pipeline: left_toks | right_toks
// Creates a pipe; left child writes to its write end (stdout),
// right child reads from its read end (stdin).
// Focus goes to the right child (the consumer).
static void run_pipe(char ltoks[][128], int lntoks,
                     char rtoks[][128], int rntoks,
                     const char* outfile)
{
    if (lntoks == 0 || rntoks == 0) return;

    int fds[2];
    if (sc(SYS_PIPE, (int)fds, 0, 0) != 0)
    {
        puts2("ush: pipe failed\n");
        return;
    }

    const char* largv[MAX_TOKS + 1];
    for (int i = 0; i < lntoks && i < MAX_TOKS; i++) largv[i] = ltoks[i];
    largv[lntoks < MAX_TOKS ? lntoks : MAX_TOKS] = 0;

    const char* rargv[MAX_TOKS + 1];
    for (int i = 0; i < rntoks && i < MAX_TOKS; i++) rargv[i] = rtoks[i];
    rargv[rntoks < MAX_TOKS ? rntoks : MAX_TOKS] = 0;

    // Fork left child: stdout → pipe write end
    int lpid = sc(SYS_FORK, 0, 0, 0);
    if (lpid == 0)
    {
        sc(SYS_DUP2, fds[1], 1, 0);
        sc(SYS_CLOSE, fds[0], 0, 0);
        sc(SYS_CLOSE, fds[1], 0, 0);
        sc(SYS_EXEC, (int)ltoks[0], (int)largv, 0);
        puts2("ush: not found: "); puts2(ltoks[0]); puts2("\n");
        sc(SYS_EXIT, 127, 0, 0);
    }

    // Fork right child: stdin ← pipe read end
    int rpid = sc(SYS_FORK, 0, 0, 0);
    if (rpid == 0)
    {
        sc(SYS_DUP2, fds[0], 0, 0);
        sc(SYS_CLOSE, fds[0], 0, 0);
        sc(SYS_CLOSE, fds[1], 0, 0);
        if (outfile[0])
        {
            int fd = sc(SYS_OPEN, (int)outfile, O_WRITE, 0);
            if (fd >= 0) { sc(SYS_DUP2, fd, 1, 0); sc(SYS_CLOSE, fd, 0, 0); }
        }
        sc(SYS_EXEC, (int)rtoks[0], (int)rargv, 0);
        puts2("ush: not found: "); puts2(rtoks[0]); puts2("\n");
        sc(SYS_EXIT, 127, 0, 0);
    }

    // Parent: release its copies of the pipe ends, give focus to right child
    sc(SYS_CLOSE, fds[0], 0, 0);
    sc(SYS_CLOSE, fds[1], 0, 0);

    if (lpid > 0 && rpid > 0)
    {
        sc(SYS_GIVE_FOREGROUND, rpid, 0, 0);
        int status = 0;
        sc(SYS_WAITPID, rpid, (int)&status, 0);
        sc(SYS_WAIT, (int)&status, 0, 0);  // collect left child
    }
    else
    {
        puts2("ush: fork failed\n");
        if (lpid > 0) { int s; sc(SYS_WAIT, (int)&s, 0, 0); }
        if (rpid > 0) { int s; sc(SYS_WAIT, (int)&s, 0, 0); }
    }
}

void _start(void)
{
    puts2("ush - user-space shell. Type a program name and args,\n");
    puts2("     optionally '> FILE' or 'CMD | CMD', or 'exit'.\n");

    char line[256];

    for (;;)
    {
        puts2("ush$ ");
        read_line(line, sizeof(line));

        // Skip blank lines
        int blank = 1;
        for (int i = 0; line[i]; i++) if (line[i] != ' ') { blank = 0; break; }
        if (blank) continue;

        if (streq(line, "exit")) break;
        if (streq(line, "help"))
        {
            puts2("Commands: PROG [ARGS] [> FILE]  or  PROG1 | PROG2  or  exit\n");
            continue;
        }

        // Find a pipe character
        int pipe_pos = -1;
        for (int i = 0; line[i]; i++)
            if (line[i] == '|') { pipe_pos = i; break; }

        if (pipe_pos >= 0)
        {
            char ltoks[MAX_TOKS][128], rtoks[MAX_TOKS][128];
            char outfile[128];
            int lntoks = tokenise(line, pipe_pos, ltoks, MAX_TOKS, outfile);
            int rntoks = tokenise(line + pipe_pos + 1,
                                  slen(line + pipe_pos + 1),
                                  rtoks, MAX_TOKS, outfile);
            run_pipe(ltoks, lntoks, rtoks, rntoks, outfile);
        }
        else
        {
            char toks[MAX_TOKS][128];
            char outfile[128];
            int ntoks = tokenise(line, slen(line), toks, MAX_TOKS, outfile);
            run_simple(toks, ntoks, outfile);
        }
    }

    puts2("ush: bye\n");
    sc(SYS_EXIT, 0, 0, 0);
    for (;;) {}
}
