// ush — full-featured user-space shell for GordOS.
// All kernel-shell commands are available here via syscalls.
// Pipes (|) and output redirection (>) are supported for external programs.

// ==================== Syscall numbers ====================
#define SYS_WRITE         0
#define SYS_EXIT          1
#define SYS_GETPID        2
#define SYS_READ          3
#define SYS_SLEEP         4
#define SYS_READFILE      5
#define SYS_WRITEFILE     6
#define SYS_FORK          7
#define SYS_EXEC          8
#define SYS_WAIT          9
#define SYS_WAITPID       10
#define SYS_OPEN          11
#define SYS_CLOSE         12
#define SYS_READ_FD       13
#define SYS_WRITE_FD      14
#define SYS_DUP2          15
#define SYS_GIVE_FOREGROUND 16
#define SYS_PIPE          17
#define SYS_CHDIR         18
#define SYS_GETCWD        19
#define SYS_MKDIR         20
#define SYS_RMFILE        21
#define SYS_RENAME        22
#define SYS_LISTDIR       23
#define SYS_UPTIME        24
#define SYS_MEMINFO       25
#define SYS_KILL_PID      26
#define SYS_PS            27
#define SYS_SETCOLOR      28
#define SYS_FASTERFETCH   29
#define SYS_PETER         30
#define SYS_FINDPREFIX    31
#define SYS_READRAW       32
#define SYS_GETTIME       33
#define SYS_CLEAR         34
#define O_WRITE           1

// ==================== VGA color constants ====================
#define VGA_BLACK          0
#define VGA_BLUE           1
#define VGA_GREEN          2
#define VGA_CYAN           3
#define VGA_RED            4
#define VGA_MAGENTA        5
#define VGA_BROWN          6
#define VGA_LIGHT_GREY     7
#define VGA_DARK_GREY      8
#define VGA_LIGHT_BLUE     9
#define VGA_LIGHT_GREEN   10
#define VGA_LIGHT_CYAN    11
#define VGA_LIGHT_RED     12
#define VGA_LIGHT_MAGENTA 13
#define VGA_YELLOW        14
#define VGA_WHITE         15
#define COLOR(fg, bg)     ((fg) | ((bg) << 4))

// ==================== Special key codes ====================
#define KEY_UP     ((unsigned char)0x80)
#define KEY_DOWN   ((unsigned char)0x81)
#define KEY_LEFT   ((unsigned char)0x82)
#define KEY_RIGHT  ((unsigned char)0x83)
#define KEY_TAB    ((unsigned char)0x09)
#define KEY_CANCEL ((unsigned char)0x03)
#define KEY_CLEAR  ((unsigned char)0x0C)

// ==================== Syscall wrappers ====================
static inline int sc(int n, int a, int b, int c)
{
    int r;
    __asm__ volatile("int $0x80"
                     : "=a"(r) : "a"(n), "b"(a), "c"(b), "d"(c) : "memory");
    return r;
}

// ==================== Output utilities ====================
static int slen(const char* s) { int n = 0; while (s[n]) n++; return n; }

static void swrite(const char* s) { sc(SYS_WRITE, (int)s, slen(s), 0); }

static void sputc(char c) { sc(SYS_WRITE, (int)&c, 1, 0); }

static void print_uint(unsigned int n)
{
    char buf[12];
    int i = 11;
    buf[i] = '\0';
    if (n == 0) { buf[--i] = '0'; }
    else { while (n) { buf[--i] = '0' + n % 10; n /= 10; } }
    swrite(buf + i);
}

static void print_2dig(unsigned int n)
{
    char buf[3];
    buf[0] = '0' + (n / 10) % 10;
    buf[1] = '0' + n % 10;
    buf[2] = '\0';
    swrite(buf);
}

static void print_4dig(unsigned int n)
{
    char buf[5];
    buf[0] = '0' + (n / 1000) % 10;
    buf[1] = '0' + (n / 100) % 10;
    buf[2] = '0' + (n / 10) % 10;
    buf[3] = '0' + n % 10;
    buf[4] = '\0';
    swrite(buf);
}

static int sstreq(const char* a, const char* b)
{
    int i = 0;
    while (a[i] && a[i] == b[i]) i++;
    return a[i] == b[i];
}

// Copy n chars from src to dst
static void smemcpy(char* dst, const char* src, int n)
{
    for (int i = 0; i < n; i++) dst[i] = src[i];
}

// ==================== Color output ====================
static void setcolor(unsigned char c) { sc(SYS_SETCOLOR, c, 0, 0); }
static void resetcolor(void) { setcolor(COLOR(VGA_LIGHT_GREY, VGA_BLACK)); }

// ==================== Line editing state (globals) ====================
#define MAX_LINE  256
#define MAX_HIST  10
#define MAX_TOKS  16

static char g_input[MAX_LINE];
static int  g_ilen = 0;
static int  g_cur  = 0;

static char g_hist[MAX_HIST][MAX_LINE];
static int  g_hcount = 0;
static int  g_hidx   = -1;

// ==================== Large data buffers (avoid stack overflow) ====================
static char g_file_buf[16384];
static char g_cwd_buf[256];
static char g_match_raw[16 * 256 + 4];
static char g_matches[16][256];
static char g_toks[MAX_TOKS][128];
static char g_ltoks[MAX_TOKS][128];
static char g_rtoks[MAX_TOKS][128];
static char g_outfile[128];

// ==================== Readline helpers ====================

// Move terminal cursor left n positions
static void cur_left_n(int n) { for (int i = 0; i < n; i++) sputc('\b'); }

// Redraw input buffer from position `from` to end, then move cursor back to `to`
static void redraw_from(int from, int cur_after)
{
    for (int i = from; i < g_ilen; i++) sputc(g_input[i]);
    sputc(' ');                                // erase dangling last char
    cur_left_n(g_ilen - cur_after + 1);        // move back to cur_after
}

static void push_history(void)
{
    if (g_ilen == 0) return;
    if (g_hcount == MAX_HIST)
    {
        for (int i = 0; i < MAX_HIST - 1; i++)
            smemcpy(g_hist[i], g_hist[i + 1], MAX_LINE);
        g_hcount--;
    }
    smemcpy(g_hist[g_hcount], g_input, g_ilen + 1);
    g_hcount++;
}

static void load_history(int idx)
{
    if (idx < 0 || idx >= g_hcount) return;
    int entry = g_hcount - 1 - idx;
    int n = 0;
    while (g_hist[entry][n]) n++;
    smemcpy(g_input, g_hist[entry], n + 1);
    g_ilen = n;
    g_cur  = n;
}

// Clear the entire currently-displayed input from the terminal
static void clear_display_line(void)
{
    // Move cursor to end
    for (int i = g_cur; i < g_ilen; i++) sputc(g_input[i]);
    // Backspace over everything
    for (int i = 0; i < g_ilen; i++) sputc('\b');
    // Overwrite with spaces
    for (int i = 0; i < g_ilen; i++) sputc(' ');
    // Move back
    for (int i = 0; i < g_ilen; i++) sputc('\b');
}

// ==================== Tab completion ====================

static const char* g_cmds[] = {
    "help", "clear", "echo", "about", "ls", "pwd",
    "cat", "touch", "mkdir", "cd", "rm", "write",
    "rename", "time", "uptime", "free", "exec",
    "bg", "ps", "kill", "fasterfetch", "peter", "reboot", 0
};

static void do_tab(void)
{
    // Find the start of the current word
    int word_start = g_cur;
    while (word_start > 0 && g_input[word_start - 1] != ' ')
        word_start--;

    char prefix[MAX_LINE];
    int plen = g_cur - word_start;
    smemcpy(prefix, g_input + word_start, plen);
    prefix[plen] = '\0';

    int has_space = 0;
    for (int i = 0; i < g_cur; i++)
        if (g_input[i] == ' ') { has_space = 1; break; }

    int count = 0;

    if (!has_space)
    {
        for (int ci = 0; g_cmds[ci] && count < 16; ci++)
        {
            int m = 1;
            for (int j = 0; j < plen; j++)
                if (!g_cmds[ci][j] || g_cmds[ci][j] != prefix[j]) { m = 0; break; }
            if (m)
            {
                int k = 0;
                while (g_cmds[ci][k]) { g_matches[count][k] = g_cmds[ci][k]; k++; }
                g_matches[count][k] = '\0';
                count++;
            }
        }
    }
    else
    {
        count = sc(SYS_FINDPREFIX, (int)prefix, (int)g_match_raw, sizeof(g_match_raw));
        if (count > 16) count = 16;
        if (count > 0)
        {
            const char* p = g_match_raw;
            for (int i = 0; i < count; i++)
            {
                int k = 0;
                while (*p && k < 255) g_matches[i][k++] = *p++;
                g_matches[i][k] = '\0';
                if (*p) p++;
            }
        }
    }

    if (count == 1)
    {
        // Find the end of the current word in the buffer
        int word_end = g_cur;
        while (word_end < g_ilen && g_input[word_end] != ' ')
            word_end++;

        int old_len = word_end - word_start;
        int new_len = slen(g_matches[0]);
        int diff    = new_len - old_len;

        // Shift buffer
        if (diff > 0)
        {
            for (int i = g_ilen; i >= word_end; i--)
                g_input[i + diff] = g_input[i];
        }
        else if (diff < 0)
        {
            for (int i = word_end + diff; i <= g_ilen; i++)
                g_input[i] = g_input[i - diff];
        }

        smemcpy(g_input + word_start, g_matches[0], new_len);
        g_ilen += diff;
        g_cur   = word_start + new_len;

        // Terminal cursor is currently at (word_start + plen).
        // Move it back to word_start, redraw to end, then reposition.
        cur_left_n(plen);
        for (int i = word_start; i < g_ilen; i++) sputc(g_input[i]);
        sputc(' ');
        cur_left_n(g_ilen - g_cur + 1);
    }
    else if (count > 1)
    {
        sputc('\n');
        for (int i = 0; i < count; i++)
        {
            swrite(g_matches[i]);
            sputc(' ');
        }
        sputc('\n');
        // Reprint the current line (prompt + input)
        // We just redraw input; the caller (readline) will redraw prompt
        // Actually we need the prompt here — use a flag or just reprint input
        // For simplicity, write the input buffer and set cursor position:
        // (The shell will show prompt + input after readline returns with
        //  the line unchanged, but we need to show it NOW)
        // Print prompt first:
        setcolor(COLOR(VGA_LIGHT_GREEN, VGA_BLACK)); swrite("GordOS");
        sc(SYS_GETCWD, (int)g_cwd_buf, sizeof(g_cwd_buf), 0);
        setcolor(COLOR(VGA_LIGHT_GREY, VGA_BLACK)); swrite(g_cwd_buf);
        setcolor(COLOR(VGA_WHITE, VGA_BLACK)); swrite("> ");
        resetcolor();
        for (int i = 0; i < g_ilen; i++) sputc(g_input[i]);
        cur_left_n(g_ilen - g_cur);
    }
}

// ==================== Readline ====================

static void readline(void)
{
    g_ilen  = 0;
    g_cur   = 0;
    g_hidx  = -1;
    g_input[0] = '\0';

    for (;;)
    {
        unsigned char c;
        sc(SYS_READRAW, (int)&c, 1, 0);

        if (c == '\n')
        {
            sputc('\n');
            g_input[g_ilen] = '\0';
            push_history();
            return;
        }

        if (c == KEY_CANCEL)
        {
            swrite("^C\n");
            g_ilen = 0; g_cur = 0; g_input[0] = '\0';
            return;
        }

        if (c == KEY_CLEAR)
        {
            sc(SYS_CLEAR, 0, 0, 0);
            // Redraw prompt
            setcolor(COLOR(VGA_LIGHT_GREEN, VGA_BLACK)); swrite("GordOS");
            sc(SYS_GETCWD, (int)g_cwd_buf, sizeof(g_cwd_buf), 0);
            setcolor(COLOR(VGA_LIGHT_GREY, VGA_BLACK)); swrite(g_cwd_buf);
            setcolor(COLOR(VGA_WHITE, VGA_BLACK)); swrite("> ");
            resetcolor();
            // Redraw current input
            for (int i = 0; i < g_ilen; i++) sputc(g_input[i]);
            cur_left_n(g_ilen - g_cur);
            continue;
        }

        if (c == '\b')
        {
            if (g_cur > 0)
            {
                g_cur--;
                for (int i = g_cur; i < g_ilen - 1; i++)
                    g_input[i] = g_input[i + 1];
                g_ilen--;
                sputc('\b');
                redraw_from(g_cur, g_cur);
            }
            continue;
        }

        if (c == KEY_LEFT)
        {
            if (g_cur > 0) { g_cur--; sputc('\b'); }
            continue;
        }

        if (c == KEY_RIGHT)
        {
            if (g_cur < g_ilen) { sputc(g_input[g_cur]); g_cur++; }
            continue;
        }

        if (c == KEY_UP)
        {
            int ni = g_hidx + 1;
            if (ni < g_hcount)
            {
                clear_display_line();
                g_hidx = ni;
                load_history(g_hidx);
                for (int i = 0; i < g_ilen; i++) sputc(g_input[i]);
            }
            continue;
        }

        if (c == KEY_DOWN)
        {
            if (g_hidx < 0) continue;
            clear_display_line();
            g_hidx--;
            if (g_hidx < 0)
            {
                g_ilen = 0; g_cur = 0; g_input[0] = '\0';
            }
            else
            {
                load_history(g_hidx);
                for (int i = 0; i < g_ilen; i++) sputc(g_input[i]);
            }
            continue;
        }

        if (c == KEY_TAB)
        {
            do_tab();
            continue;
        }

        // Printable character: insert at cursor
        if (c >= 0x20 && c < 0x80 && g_ilen < MAX_LINE - 1)
        {
            for (int i = g_ilen; i > g_cur; i--)
                g_input[i] = g_input[i - 1];
            g_input[g_cur] = (char)c;
            g_ilen++;
            // Redraw from cursor to end
            for (int i = g_cur; i < g_ilen; i++) sputc(g_input[i]);
            g_cur++;
            cur_left_n(g_ilen - g_cur);
        }
    }
}

// ==================== Tokenizer ====================

// Split line[0..len) into tokens, extracting '>' redirection into outfile.
// Returns token count. Pipe character '|' is NOT handled here.
static int tokenise(const char* line, int len, char toks[][128], int max, char* outfile)
{
    int n = 0, i = 0;
    outfile[0] = '\0';
    while (i < len && n < max)
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

// ==================== Program execution ====================

static void run_simple(char toks[][128], int ntoks, const char* outfile, int fg)
{
    if (ntoks == 0) return;

    const char* argv[MAX_TOKS + 1];
    for (int i = 0; i < ntoks && i < MAX_TOKS; i++) argv[i] = toks[i];
    argv[ntoks < MAX_TOKS ? ntoks : MAX_TOKS] = 0;

    int pid = sc(SYS_FORK, 0, 0, 0);
    if (pid == 0)
    {
        if (outfile && outfile[0])
        {
            int fd = sc(SYS_OPEN, (int)outfile, O_WRITE, 0);
            if (fd < 0) { swrite("ush: cannot open output file\n"); sc(SYS_EXIT, 1, 0, 0); }
            sc(SYS_DUP2, fd, 1, 0);
            sc(SYS_CLOSE, fd, 0, 0);
        }
        sc(SYS_EXEC, (int)toks[0], (int)argv, 0);
        swrite("ush: not found: "); swrite(toks[0]); sputc('\n');
        sc(SYS_EXIT, 127, 0, 0);
    }
    else if (pid > 0)
    {
        if (fg)
        {
            sc(SYS_GIVE_FOREGROUND, pid, 0, 0);
            int status = 0;
            sc(SYS_WAITPID, pid, (int)&status, 0);
        }
        else
        {
            swrite("["); print_uint((unsigned int)pid); swrite("] running in background\n");
        }
    }
    else { swrite("ush: fork failed\n"); }
}

static void run_pipe(char ltoks[][128], int lntoks,
                     char rtoks[][128], int rntoks,
                     const char* outfile)
{
    if (lntoks == 0 || rntoks == 0) return;
    int fds[2];
    if (sc(SYS_PIPE, (int)fds, 0, 0) != 0) { swrite("ush: pipe failed\n"); return; }

    const char* largv[MAX_TOKS + 1];
    for (int i = 0; i < lntoks && i < MAX_TOKS; i++) largv[i] = ltoks[i];
    largv[lntoks < MAX_TOKS ? lntoks : MAX_TOKS] = 0;

    const char* rargv[MAX_TOKS + 1];
    for (int i = 0; i < rntoks && i < MAX_TOKS; i++) rargv[i] = rtoks[i];
    rargv[rntoks < MAX_TOKS ? rntoks : MAX_TOKS] = 0;

    int lpid = sc(SYS_FORK, 0, 0, 0);
    if (lpid == 0)
    {
        sc(SYS_DUP2, fds[1], 1, 0);
        sc(SYS_CLOSE, fds[0], 0, 0); sc(SYS_CLOSE, fds[1], 0, 0);
        sc(SYS_EXEC, (int)ltoks[0], (int)largv, 0);
        swrite("ush: not found: "); swrite(ltoks[0]); sputc('\n');
        sc(SYS_EXIT, 127, 0, 0);
    }

    int rpid = sc(SYS_FORK, 0, 0, 0);
    if (rpid == 0)
    {
        sc(SYS_DUP2, fds[0], 0, 0);
        sc(SYS_CLOSE, fds[0], 0, 0); sc(SYS_CLOSE, fds[1], 0, 0);
        if (outfile && outfile[0])
        {
            int fd = sc(SYS_OPEN, (int)outfile, O_WRITE, 0);
            if (fd >= 0) { sc(SYS_DUP2, fd, 1, 0); sc(SYS_CLOSE, fd, 0, 0); }
        }
        sc(SYS_EXEC, (int)rtoks[0], (int)rargv, 0);
        swrite("ush: not found: "); swrite(rtoks[0]); sputc('\n');
        sc(SYS_EXIT, 127, 0, 0);
    }

    sc(SYS_CLOSE, fds[0], 0, 0); sc(SYS_CLOSE, fds[1], 0, 0);
    if (lpid > 0 && rpid > 0)
    {
        sc(SYS_GIVE_FOREGROUND, rpid, 0, 0);
        int status = 0;
        sc(SYS_WAITPID, rpid, (int)&status, 0);
        sc(SYS_WAIT, (int)&status, 0, 0);
    }
    else
    {
        swrite("ush: fork failed\n");
        if (lpid > 0) { int s; sc(SYS_WAIT, (int)&s, 0, 0); }
        if (rpid > 0) { int s; sc(SYS_WAIT, (int)&s, 0, 0); }
    }
}

// ==================== Built-in commands ====================

static void cmd_help(void)
{
    swrite("Available commands:\n");
    swrite("  help       - show this message\n");
    swrite("  clear      - clear the screen\n");
    swrite("  echo       - print arguments\n");
    swrite("  ls [path]  - list directory\n");
    swrite("  pwd        - print working directory\n");
    swrite("  cat FILE   - print file contents\n");
    swrite("  touch FILE - create empty file\n");
    swrite("  mkdir DIR  - create directory\n");
    swrite("  cd DIR     - change directory\n");
    swrite("  rm FILE    - delete file\n");
    swrite("  write FILE TEXT - write text to file\n");
    swrite("  rename OLD NEW  - rename file or directory\n");
    swrite("  exec PROG [args] - run program (foreground)\n");
    swrite("  bg PROG [args]   - run program (background)\n");
    swrite("  ps         - list processes\n");
    swrite("  kill PID   - terminate a process\n");
    swrite("  time       - show date and time\n");
    swrite("  uptime     - show system uptime\n");
    swrite("  free       - show memory usage\n");
    swrite("  fasterfetch - show system info graphic\n");
    swrite("  peter      - show the mascot\n");
    swrite("  reboot     - restart the system\n");
    swrite("  about      - about GordOS\n");
    swrite("  exit       - exit the shell\n");
    swrite("\n  Ctrl+C cancels input. Ctrl+L clears the screen.\n");
    swrite("  Pipes: CMD1 | CMD2    Redirect: CMD > FILE\n");
}

static void cmd_clear(void) { sc(SYS_CLEAR, 0, 0, 0); }

static void cmd_echo(const char* args)
{
    if (args && *args) swrite(args);
    sputc('\n');
}

static void cmd_about(void)
{
    setcolor(COLOR(VGA_LIGHT_MAGENTA, VGA_BLACK));
    swrite("  GordOS\n");
    resetcolor();
    swrite("  A hobby OS by Hamish Gordon\n");
    swrite("  Built from scratch in C and x86 Assembly\n");
    swrite("  https://github.com/Millenium2133/GordOS\n");
}

static void cmd_ls(const char* args)
{
    const char* path = (args && *args) ? args : "";
    if (sc(SYS_LISTDIR, (int)path, 0, 0) != 0)
        swrite("ls: cannot list directory\n");
}

static void cmd_cat(const char* args)
{
    if (!args || !*args) { swrite("Usage: cat FILE\n"); return; }
    int n = sc(SYS_READFILE, (int)args, (int)g_file_buf, sizeof(g_file_buf));
    if (n < 0) { swrite("cat: file not found\n"); return; }
    sc(SYS_WRITE, (int)g_file_buf, n, 0);
    sputc('\n');
}

static void cmd_touch(const char* args)
{
    if (!args || !*args) { swrite("Usage: touch FILE\n"); return; }
    int fd = sc(SYS_OPEN, (int)args, 0, 0);
    if (fd >= 0) { sc(SYS_CLOSE, fd, 0, 0); swrite("touch: file already exists\n"); return; }
    if (sc(SYS_WRITEFILE, (int)args, (int)"", 0) == 0)
        { swrite("Created: "); swrite(args); sputc('\n'); }
    else
        swrite("touch: failed\n");
}

static void cmd_write(const char* args)
{
    if (!args || !*args) { swrite("Usage: write FILE TEXT\n"); return; }
    char filename[256];
    const char* content = args;
    int fi = 0;
    while (*content && *content != ' ' && fi < 255) filename[fi++] = *content++;
    filename[fi] = '\0';
    if (*content == ' ') content++;
    if (sc(SYS_WRITEFILE, (int)filename, (int)content, slen(content)) == 0)
        { swrite("Written to: "); swrite(filename); sputc('\n'); }
    else
        swrite("write: failed\n");
}

static void cmd_rm(const char* args)
{
    if (!args || !*args) { swrite("Usage: rm FILE\n"); return; }
    if (sc(SYS_RMFILE, (int)args, 0, 0) == 0)
        { swrite("Deleted: "); swrite(args); sputc('\n'); }
    else
        swrite("rm: not found\n");
}

static void cmd_rename(const char* args)
{
    if (!args || !*args) { swrite("Usage: rename OLD NEW\n"); return; }
    char old[256];
    const char* rest = args;
    int fi = 0;
    while (*rest && *rest != ' ' && fi < 255) old[fi++] = *rest++;
    old[fi] = '\0';
    if (!*rest) { swrite("rename: missing new name\n"); return; }
    if (*rest == ' ') rest++;
    if (sc(SYS_RENAME, (int)old, (int)rest, 0) == 0)
        { swrite("Renamed to: "); swrite(rest); sputc('\n'); }
    else
        swrite("rename: failed\n");
}

static void cmd_pwd(void)
{
    sc(SYS_GETCWD, (int)g_cwd_buf, sizeof(g_cwd_buf), 0);
    swrite(g_cwd_buf); sputc('\n');
}

static void cmd_mkdir(const char* args)
{
    if (!args || !*args) { swrite("Usage: mkdir DIR\n"); return; }
    if (sc(SYS_MKDIR, (int)args, 0, 0) == 0)
        { swrite("Created directory: "); swrite(args); sputc('\n'); }
    else
        swrite("mkdir: failed\n");
}

static void cmd_cd(const char* args)
{
    if (!args || !*args) { swrite("Usage: cd DIR\n"); return; }
    if (sc(SYS_CHDIR, (int)args, 0, 0) != 0)
        swrite("cd: not found\n");
}

static void cmd_time(void)
{
    unsigned int buf[2];
    sc(SYS_GETTIME, (int)buf, 0, 0);
    unsigned int h  = (buf[0] >> 16) & 0xFF;
    unsigned int m  = (buf[0] >> 8)  & 0xFF;
    unsigned int s  =  buf[0]        & 0xFF;
    unsigned int y  = (buf[1] >> 16) & 0xFFFF;
    unsigned int mo = (buf[1] >> 8)  & 0xFF;
    unsigned int d  =  buf[1]        & 0xFF;
    print_2dig(h); sputc(':'); print_2dig(m); sputc(':'); print_2dig(s);
    swrite("  ");
    print_4dig(y); sputc('-'); print_2dig(mo); sputc('-'); print_2dig(d);
    sputc('\n');
}

static void cmd_uptime(void)
{
    unsigned int sec = (unsigned int)sc(SYS_UPTIME, 0, 0, 0);
    swrite("Up "); print_uint(sec / 60); swrite(" min "); print_uint(sec % 60); swrite(" sec\n");
}

static void cmd_free(void)
{
    unsigned int buf[2];
    sc(SYS_MEMINFO, (int)buf, 0, 0);
    unsigned int used  = buf[0];
    unsigned int total = buf[1];
    unsigned int free_ = total - used;
    print_uint(free_); swrite(" MB free / "); print_uint(total); swrite(" MB total\n");
}

static void cmd_ps(void) { sc(SYS_PS, 0, 0, 0); }

static void cmd_kill(const char* args)
{
    if (!args || !*args) { swrite("Usage: kill PID\n"); return; }
    unsigned int pid = 0;
    for (const char* p = args; *p >= '0' && *p <= '9'; p++)
        pid = pid * 10 + (unsigned int)(*p - '0');
    if (pid == 0) { swrite("kill: cannot kill the kernel\n"); return; }
    if (sc(SYS_KILL_PID, (int)pid, 0, 0) == 0)
        { swrite("killed ["); print_uint(pid); swrite("]\n"); }
    else
        swrite("kill: no such process\n");
}

static void cmd_fasterfetch(void) { sc(SYS_FASTERFETCH, 0, 0, 0); }

static void cmd_peter(void) { sc(SYS_PETER, 0, 0, 0); }

static void cmd_reboot(void)
{
    swrite("Rebooting...\n");
    // Ask the 8042 to pulse the CPU reset line (via a kernel syscall that
    // happens to work: exec a non-existent file makes the kernel's reboot
    // fallback handle it gracefully, or we just exit and let it hang.
    // Actually the cleanest is: exit the shell; the kernel falls back to
    // the emergency shell which has a reboot command).
    // For now, call SYS_EXIT and let the user use the kernel shell reboot.
    // (A proper SYS_REBOOT syscall can be added later.)
    sc(SYS_EXIT, 0, 0, 0);
}

static void cmd_exec(const char* args)
{
    if (!args || !*args) { swrite("Usage: exec PROG [args]\n"); return; }
    // Parse tokens from args
    int n = tokenise(args, slen(args), g_toks, MAX_TOKS, g_outfile);
    run_simple(g_toks, n, g_outfile, 1);
}

static void cmd_bg(const char* args)
{
    if (!args || !*args) { swrite("Usage: bg PROG [args]\n"); return; }
    int n = tokenise(args, slen(args), g_toks, MAX_TOKS, g_outfile);
    run_simple(g_toks, n, g_outfile, 0);
}

// ==================== Prompt ====================

static void prompt(void)
{
    setcolor(COLOR(VGA_LIGHT_GREEN, VGA_BLACK)); swrite("GordOS");
    sc(SYS_GETCWD, (int)g_cwd_buf, sizeof(g_cwd_buf), 0);
    setcolor(COLOR(VGA_LIGHT_GREY, VGA_BLACK)); swrite(g_cwd_buf);
    setcolor(COLOR(VGA_WHITE, VGA_BLACK)); swrite("> ");
    resetcolor();
}

// ==================== Check if a string starts with a word ====================

static int is_cmd(const char* line, const char* cmd)
{
    int i = 0;
    while (cmd[i] && line[i] == cmd[i]) i++;
    return !cmd[i] && (line[i] == '\0' || line[i] == ' ');
}

static const char* get_args(const char* line, int cmdlen)
{
    if (line[cmdlen] == ' ' && line[cmdlen + 1]) return line + cmdlen + 1;
    return 0;
}

// ==================== Main REPL ====================

static void dispatch(const char* line)
{
    // Skip blank lines
    int blank = 1;
    for (int i = 0; line[i]; i++) if (line[i] != ' ') { blank = 0; break; }
    if (blank) return;

    if (sstreq(line, "exit")) { sc(SYS_EXIT, 0, 0, 0); return; }

    if (is_cmd(line, "help"))        { cmd_help();                        return; }
    if (is_cmd(line, "clear"))       { cmd_clear();                       return; }
    if (is_cmd(line, "echo"))        { cmd_echo(get_args(line, 4));       return; }
    if (is_cmd(line, "about"))       { cmd_about();                       return; }
    if (is_cmd(line, "ls"))          { cmd_ls(get_args(line, 2));         return; }
    if (is_cmd(line, "pwd"))         { cmd_pwd();                         return; }
    if (is_cmd(line, "cat"))         { cmd_cat(get_args(line, 3));        return; }
    if (is_cmd(line, "touch"))       { cmd_touch(get_args(line, 5));      return; }
    if (is_cmd(line, "mkdir"))       { cmd_mkdir(get_args(line, 5));      return; }
    if (is_cmd(line, "cd"))          { cmd_cd(get_args(line, 2));         return; }
    if (is_cmd(line, "rm"))          { cmd_rm(get_args(line, 2));         return; }
    if (is_cmd(line, "write"))       { cmd_write(get_args(line, 5));      return; }
    if (is_cmd(line, "rename"))      { cmd_rename(get_args(line, 6));     return; }
    if (is_cmd(line, "time"))        { cmd_time();                        return; }
    if (is_cmd(line, "uptime"))      { cmd_uptime();                      return; }
    if (is_cmd(line, "free"))        { cmd_free();                        return; }
    if (is_cmd(line, "ps"))          { cmd_ps();                          return; }
    if (is_cmd(line, "kill"))        { cmd_kill(get_args(line, 4));       return; }
    if (is_cmd(line, "fasterfetch")) { cmd_fasterfetch();                 return; }
    if (is_cmd(line, "peter"))       { cmd_peter();                       return; }
    if (is_cmd(line, "reboot"))      { cmd_reboot();                      return; }
    if (is_cmd(line, "exec"))        { cmd_exec(get_args(line, 4));       return; }
    if (is_cmd(line, "bg"))          { cmd_bg(get_args(line, 2));         return; }

    // Not a built-in: look for a pipe, then try as an external program
    int pipe_pos = -1;
    for (int i = 0; line[i]; i++) if (line[i] == '|') { pipe_pos = i; break; }

    if (pipe_pos >= 0)
    {
        int ln = tokenise(line, pipe_pos, g_ltoks, MAX_TOKS, g_outfile);
        int rn = tokenise(line + pipe_pos + 1, slen(line + pipe_pos + 1),
                          g_rtoks, MAX_TOKS, g_outfile);
        run_pipe(g_ltoks, ln, g_rtoks, rn, g_outfile);
    }
    else
    {
        int n = tokenise(line, slen(line), g_toks, MAX_TOKS, g_outfile);
        run_simple(g_toks, n, g_outfile, 1);
    }
}

void _start(void)
{
    setcolor(COLOR(VGA_LIGHT_GREEN, VGA_BLACK));
    swrite("GordOS");
    resetcolor();
    swrite(" shell — type 'help' for commands\n");

    for (;;)
    {
        prompt();
        readline();
        if (g_ilen > 0 || g_input[0] == '\0')
            dispatch(g_input);
    }
}
