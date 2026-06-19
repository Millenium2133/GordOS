#include "syscall.h"
#include "vga.h"
#include "process.h"
#include "scheduler.h"
#include "keyboard.h"
#include "pit.h"
#include "fat32.h"
#include "kmalloc.h"
#include "wbuf.h"
#include "pipe.h"

void terminal_writestring(const char* data);
void terminal_putchar(char c);
void terminal_backspace(void);

// Check that [buf, buf+len) lies entirely in user space
static int user_range_ok(const void* buf, uint32_t len)
{
    uint32_t start = (uint32_t)buf;
    return buf && start < 0xC0000000 && len <= 0xC0000000 - start;
}

// Write len bytes to the terminal
static int tty_write(const char* buf, uint32_t len)
{
    for (uint32_t i = 0; i < len; i++)
        terminal_putchar(buf[i]);
    return (int)len;
}

// Append to a writable file fd's buffer (flushed to disk on close)
static int file_write(file_desc_t* f, const char* buf, uint32_t len)
{
    if (!f->writable || !f->wbuf)
        return -1;
    return wbuf_write(f->wbuf, buf, len);
}

// Write bytes into a pipe, blocking if the buffer is full. Wakes a
// blocked reader after each write. Returns broken-pipe (-1) if all
// read ends are closed.
static int pipe_write_fd(pipe_t* p, const char* buf, uint32_t len)
{
    if (!p->read_open)
        return -1;

    uint32_t written = 0;
    while (written < len)
    {
        int used  = (p->head - p->tail + PIPE_BUF_SIZE) % PIPE_BUF_SIZE;
        int space = PIPE_BUF_SIZE - 1 - used;

        if (space > 0)
        {
            uint32_t n = (uint32_t)space < (len - written)
                         ? (uint32_t)space : (len - written);
            for (uint32_t i = 0; i < n; i++)
            {
                p->buf[p->head] = buf[written + i];
                p->head = (p->head + 1) % PIPE_BUF_SIZE;
            }
            written += n;
            if (p->blocked_reader)
            {
                scheduler_wake(p->blocked_reader);
                p->blocked_reader = 0;
            }
        }
        else if (!p->read_open)
        {
            break;
        }
        else
        {
            p->blocked_writer = current_process;
            scheduler_block(BLOCK_PIPE_WRITE, 0);
            scheduler_switch();
        }
    }
    return (int)written;
}

// Write to an fd, dispatching on its kind. stdout/stderr go to the
// terminal; a writable file fd accumulates into its buffer.
static int do_fd_write(int fd, const char* buf, uint32_t len)
{
    process_t* me = current_process;
    if (!me || fd < 0 || fd >= MAX_FDS || me->fds[fd].kind == FD_NONE)
        return -1;
    if (!user_range_ok(buf, len))
        return -1;

    file_desc_t* f = &me->fds[fd];
    if (f->kind == FD_TTY_OUT)
        return tty_write(buf, len);
    if (f->kind == FD_FILE)
        return file_write(f, buf, len);
    if (f->kind == FD_PIPE_WRITE)
        return pipe_write_fd((pipe_t*)f->pipe, buf, len);
    return -1; // e.g. writing to stdin
}

// sys_write: write ecx bytes from ebx to stdout (fd 1). Routing through
// the fd table is what lets `cmd > file` redirect a program's output
// without the program knowing.
static int sys_write(const char* buf, uint32_t len)
{
    return do_fd_write(1, buf, len);
}

// Blocking keyboard read into buf. Drains the ring buffer; if empty,
// sleeps on the scheduler (BLOCK_READ) until the keyboard handler wakes
// us. We run with interrupts disabled (syscall context), so draining and
// then deciding to block is atomic against the keyboard IRQ — no lost
// wakeup. Consumed characters are echoed.
static int tty_read(char* buf, uint32_t len)
{
    uint32_t got = 0;

    for (;;)
    {
        int c;
        while (got < len && (c = keyboard_read_char()) >= 0)
        {
            buf[got++] = (char)c;

            if (c == '\b')
                terminal_backspace();
            else
                terminal_putchar((char)c);
        }

        if (got > 0)
            break;

        scheduler_block(BLOCK_READ, 0);
        scheduler_switch();
    }

    return (int)got;
}

// Read from a file fd, resuming from its cached cluster position.
// Returns bytes read (0 at EOF).
static int file_read(file_desc_t* f, char* buf, uint32_t len)
{
    if (f->writable)
        return -1; // opened for writing

    uint32_t cbytes = fat32_cluster_size();

    uint8_t* cluster_buf = kmalloc(cbytes);
    if (!cluster_buf)
        return -1;

    uint32_t got = 0;
    while (got < len && f->pos < f->size &&
           f->cur_cluster >= 2 && f->cur_cluster < FAT32_EOC)
    {
        if (fat32_read_cluster(f->cur_cluster, cluster_buf) != 0)
            break;

        uint32_t avail = cbytes - f->cluster_offset;   // left in this cluster
        uint32_t left  = f->size - f->pos;             // left in the file
        uint32_t want  = len - got;                    // left to satisfy
        uint32_t n = want;
        if (n > avail) n = avail;
        if (n > left)  n = left;

        for (uint32_t i = 0; i < n; i++)
            buf[got + i] = (char)cluster_buf[f->cluster_offset + i];

        got               += n;
        f->pos            += n;
        f->cluster_offset += n;

        if (f->cluster_offset >= cbytes)
        {
            f->cur_cluster    = fat32_next_cluster(f->cur_cluster);
            f->cluster_offset = 0;
        }
    }

    kfree(cluster_buf);
    return (int)got;
}

// Read bytes from a pipe, blocking while it's empty (but the write end
// is still open). Returns 0 when empty and write end is fully closed
// (EOF). Returns the number of bytes read otherwise.
static int pipe_read_fd(pipe_t* p, char* buf, uint32_t len)
{
    for (;;)
    {
        int avail = (p->head - p->tail + PIPE_BUF_SIZE) % PIPE_BUF_SIZE;
        if (avail > 0)
        {
            uint32_t n = (uint32_t)avail < len ? (uint32_t)avail : len;
            for (uint32_t i = 0; i < n; i++)
            {
                buf[i] = p->buf[p->tail];
                p->tail = (p->tail + 1) % PIPE_BUF_SIZE;
            }
            if (p->blocked_writer)
            {
                scheduler_wake(p->blocked_writer);
                p->blocked_writer = 0;
            }
            return (int)n;
        }

        if (!p->write_open)
            return 0; // EOF

        p->blocked_reader = current_process;
        scheduler_block(BLOCK_PIPE_READ, 0);
        scheduler_switch();
    }
}

// Read from an fd, dispatching on its kind.
static int do_fd_read(int fd, char* buf, uint32_t len)
{
    process_t* me = current_process;
    if (!me || fd < 0 || fd >= MAX_FDS || me->fds[fd].kind == FD_NONE)
        return -1;
    if (!user_range_ok(buf, len))
        return -1;
    if (len == 0)
        return 0;

    file_desc_t* f = &me->fds[fd];
    if (f->kind == FD_TTY_IN)
        return tty_read(buf, len);
    if (f->kind == FD_FILE)
        return file_read(f, buf, len);
    if (f->kind == FD_PIPE_READ)
        return pipe_read_fd((pipe_t*)f->pipe, buf, len);
    return -1; // e.g. reading from stdout
}

// sys_read: read up to ecx bytes from stdin (fd 0) into ebx.
static int sys_read(char* buf, uint32_t len)
{
    return do_fd_read(0, buf, len);
}

// sys_sleep: block for ebx milliseconds
static int sys_sleep(uint32_t ms)
{
    // Re-enable interrupts so PIT ticks keep counting while we wait
    asm volatile("sti");
    timer_sleep(ms);
    asm volatile("cli");
    return 0;
}

// Copy a NUL-terminated path from user space into a kernel buffer,
// validating every byte stays below the kernel half.
// Returns 0 on success, -1 on bad/overlong path.
static int copy_user_path(const char* upath, char* out, uint32_t outsize)
{
    uint32_t i;
    for (i = 0; i < outsize - 1; i++)
    {
        if (!user_range_ok(upath + i, 1))
            return -1;
        out[i] = upath[i];
        if (out[i] == '\0')
            return 0;
    }
    return -1; // unterminated or too long
}

// sys_readfile: read the file named by ebx into buffer ecx (max edx
// bytes). Returns the number of bytes read, or -1.
static int sys_readfile(const char* upath, char* buf, uint32_t max)
{
    char path[256];
    if (copy_user_path(upath, path, sizeof(path)) != 0)
        return -1;
    if (!user_range_ok(buf, max))
        return -1;

    uint32_t size = 0;
    if (fat32_read_file(path, buf, max, &size) != 0)
        return -1;

    return (int)size;
}

// sys_writefile: write ecx (buffer) of edx bytes to the file named by
// ebx, replacing any existing contents. Returns 0 on success, -1.
static int sys_writefile(const char* upath, const char* buf, uint32_t len)
{
    char path[256];
    if (copy_user_path(upath, path, sizeof(path)) != 0)
        return -1;
    if (!user_range_ok(buf, len))
        return -1;

    return fat32_write_file(path, buf, len);
}

// O_* flags for sys_open (ecx). Default (0) is read-only; O_WRITE
// creates/truncates the file and opens it for writing.
#define O_WRITE 1

// sys_open: open the file named by ebx. Returns a small fd index, or -1
// if the file can't be opened or there's no free slot. Read opens look
// the file up; write opens create a fresh (truncating) write buffer.
static int sys_open(const char* upath, uint32_t flags)
{
    char path[256];
    if (copy_user_path(upath, path, sizeof(path)) != 0)
        return -1;

    process_t* me = current_process;
    if (!me)
        return -1;

    // Find a free slot (0/1/2 are the standard streams, so this lands
    // at fd 3+ unless one was closed)
    int fd = -1;
    for (int i = 0; i < MAX_FDS; i++)
        if (me->fds[i].kind == FD_NONE) { fd = i; break; }
    if (fd < 0)
        return -1;

    file_desc_t* f = &me->fds[fd];

    if (flags & O_WRITE)
    {
        wbuf_t* w = wbuf_open(path);
        if (!w)
            return -1;
        f->kind     = FD_FILE;
        f->writable = 1;
        f->wbuf     = w;
        return fd;
    }

    uint32_t first_cluster = 0, size = 0;
    if (fat32_lookup_file(path, &first_cluster, &size) != 0)
        return -1;

    f->kind          = FD_FILE;
    f->writable      = 0;
    f->wbuf          = 0;
    f->first_cluster = first_cluster;
    f->cur_cluster   = first_cluster;
    f->cluster_offset = 0;
    f->pos           = 0;
    f->size          = size;
    return fd;
}

// sys_read_fd / sys_write_fd: explicit-fd variants of read/write.
static int sys_read_fd(int fd, char* buf, uint32_t len)
{
    return do_fd_read(fd, buf, len);
}

static int sys_write_fd(int fd, const char* buf, uint32_t len)
{
    return do_fd_write(fd, buf, len);
}

// sys_close: release an fd. A writable fd flushes to disk when its last
// reference is dropped; a pipe end wakes any blocked peer. Returns 0
// on success, -1 on a bad fd.
static int sys_close(int fd)
{
    process_t* me = current_process;
    if (!me || fd < 0 || fd >= MAX_FDS || me->fds[fd].kind == FD_NONE)
        return -1;

    if (me->fds[fd].kind == FD_FILE && me->fds[fd].writable)
        wbuf_unref(me->fds[fd].wbuf);

    if (me->fds[fd].kind == FD_PIPE_READ && me->fds[fd].pipe)
    {
        pipe_t* p = (pipe_t*)me->fds[fd].pipe;
        pipe_close_read(p);
        if (p->blocked_writer) { scheduler_wake(p->blocked_writer); p->blocked_writer = 0; }
        if (p->read_open <= 0 && p->write_open <= 0) pipe_destroy(p);
    }
    else if (me->fds[fd].kind == FD_PIPE_WRITE && me->fds[fd].pipe)
    {
        pipe_t* p = (pipe_t*)me->fds[fd].pipe;
        pipe_close_write(p);
        if (p->blocked_reader) { scheduler_wake(p->blocked_reader); p->blocked_reader = 0; }
        if (p->read_open <= 0 && p->write_open <= 0) pipe_destroy(p);
    }

    me->fds[fd].kind     = FD_NONE;
    me->fds[fd].writable = 0;
    me->fds[fd].wbuf     = 0;
    me->fds[fd].pipe     = 0;
    return 0;
}

// sys_dup2: make newfd refer to the same open file as oldfd, closing
// whatever newfd was. This is how the shell redirects: open a file,
// dup2 it onto fd 1, and the child's writes to stdout land in the file.
// Returns newfd, or -1 on a bad fd.
static int sys_dup2(int oldfd, int newfd)
{
    process_t* me = current_process;
    if (!me || oldfd < 0 || oldfd >= MAX_FDS || newfd < 0 || newfd >= MAX_FDS)
        return -1;
    if (me->fds[oldfd].kind == FD_NONE)
        return -1;
    if (oldfd == newfd)
        return newfd;

    // Close whatever newfd currently holds (flush a writable buffer, or
    // release a pipe end)
    sys_close(newfd);

    me->fds[newfd] = me->fds[oldfd];

    // Both fds now share a resource — bump the new reference count
    if (me->fds[newfd].kind == FD_FILE && me->fds[newfd].writable)
        wbuf_ref(me->fds[newfd].wbuf);
    if (me->fds[newfd].kind == FD_PIPE_READ && me->fds[newfd].pipe)
        ((pipe_t*)me->fds[newfd].pipe)->read_open++;
    if (me->fds[newfd].kind == FD_PIPE_WRITE && me->fds[newfd].pipe)
        ((pipe_t*)me->fds[newfd].pipe)->write_open++;

    return newfd;
}

// sys_exec: replace the calling program with the one named by ebx.
// ecx is an optional NULL-terminated user-space argv array (char**);
// pass 0 / NULL to use just the path as argv[0].
// Reads the file into a kernel buffer, builds a cmdline from the argv,
// then hands everything to process_exec which never returns on success.
static int sys_exec(const char* upath, const char** uargv)
{
    char path[256];
    if (copy_user_path(upath, path, sizeof(path)) != 0)
        return -1;

    // Build a space-delimited cmdline from the argv array so
    // paging_build_user_stack can tokenise it back into argc/argv.
    char cmdline[512];
    int  clen = 0;

    if (uargv && user_range_ok(uargv, sizeof(char*)))
    {
        for (int i = 0; i < 16; i++)
        {
            // Validate each pointer element lives in user space
            if (!user_range_ok(uargv + i, sizeof(char*)))
                break;
            const char* uarg = uargv[i];
            if (!uarg)
                break;

            char argbuf[256];
            if (copy_user_path(uarg, argbuf, sizeof(argbuf)) != 0)
                break;

            if (clen > 0 && clen < 510)
                cmdline[clen++] = ' ';
            for (int j = 0; argbuf[j] && clen < 510; j++)
                cmdline[clen++] = argbuf[j];
        }
    }

    if (clen == 0)
    {
        for (int i = 0; path[i] && clen < 510; i++)
            cmdline[clen++] = path[i];
    }
    cmdline[clen] = '\0';

    void* buf = kmalloc(65536);
    if (!buf)
        return -1;

    uint32_t size = 0;
    if (fat32_read_file(path, buf, 65536, &size) != 0)
    {
        kfree(buf);
        return -1;
    }

    // process_exec takes ownership of buf (frees it). Only returns on
    // failure, with the caller's program still intact.
    return process_exec(current_process, buf, size, cmdline);
}

// sys_give_foreground: hand keyboard focus to the process with the given
// pid. The calling process loses focus; the target gains it. This lets a
// user-space shell give keyboard input to a child it just forked.
static int sys_give_foreground(uint32_t pid)
{
    process_t* target = scheduler_find_any(pid);
    if (!target)
        return -1;

    // Transfer focus
    target->foreground = 1;
    foreground_process = target;
    return 0;
}

// sys_pipe: create an anonymous pipe. ebx points to a user int[2] that
// receives [read_fd, write_fd]. Returns 0 on success, -1 on failure.
static int sys_pipe(int* user_fds)
{
    if (!user_range_ok(user_fds, 2 * sizeof(int)))
        return -1;

    process_t* me = current_process;
    if (!me)
        return -1;

    // Find two free fd slots
    int rfd = -1, wfd = -1;
    for (int i = 0; i < MAX_FDS; i++)
    {
        if (me->fds[i].kind == FD_NONE)
        {
            if (rfd < 0)       rfd = i;
            else if (wfd < 0) { wfd = i; break; }
        }
    }
    if (rfd < 0 || wfd < 0)
        return -1;

    pipe_t* p = pipe_create();
    if (!p)
        return -1;

    me->fds[rfd].kind = FD_PIPE_READ;
    me->fds[rfd].pipe = p;
    me->fds[wfd].kind = FD_PIPE_WRITE;
    me->fds[wfd].pipe = p;

    user_fds[0] = rfd;
    user_fds[1] = wfd;
    return 0;
}

// sys_exit: tear down the calling process. The scheduler switches to
// another task and the kernel task's reaper frees this one's memory.
static void sys_exit(int code)
{
    // Stash the exit code so a waiting parent can read it
    if (current_process)
        current_process->exit_code = code;

    process_exit(); // never returns for a real process

    // Only reached if somehow called outside a process context
    asm volatile("sti");
    for (;;)
        asm volatile("hlt");
}

// sys_wait / sys_waitpid: block until a child exits, then return its pid
// and write its exit code through the user int pointer (may be 0/NULL).
// target = 0 waits for any child; otherwise a specific child pid.
// Returns the child's pid, or -1 if there is no such child to wait for.
static int sys_wait(uint32_t target, int* status)
{
    if (status && !user_range_ok(status, sizeof(int)))
        return -1;

    process_t* me = current_process;
    if (!me)
        return -1;

    // We are in syscall context, so interrupts are already disabled —
    // the exit-record and scheduler state we touch stays consistent.
    for (;;)
    {
        uint32_t cpid;
        int code;
        if (process_reap_child(me->pid, target, &cpid, &code) == 0)
        {
            if (status)
                *status = code;
            return (int)cpid;
        }

        // Nothing exited yet — is there a live child to wait on?
        if (target == 0)
        {
            if (!scheduler_has_child(me->pid))
                return -1; // no children at all
        }
        else
        {
            process_t* child = scheduler_find_any(target);
            if (!child || child->parent_pid != me->pid)
                return -1; // not our child (or already reaped)
        }

        // Sleep until a matching child exits and wakes us, then retry.
        scheduler_block(BLOCK_WAIT, target);
        scheduler_switch();
    }
}

// sys_getpid: return the current process's PID (0 if none)
static int sys_getpid(void)
{
    return current_process ? (int)current_process->pid : 0;
}

void syscall_handler(struct registers* regs)
{
    int ret = 0;

    switch (regs->eax)
    {
        case SYS_WRITE:
            ret = sys_write((const char*)regs->ebx, regs->ecx);
            break;
        case SYS_EXIT:
            sys_exit((int)regs->ebx);
            break;
        case SYS_GETPID:
            ret = sys_getpid();
            break;
        case SYS_READ:
            ret = sys_read((char*)regs->ebx, regs->ecx);
            break;
        case SYS_SLEEP:
            ret = sys_sleep(regs->ebx);
            break;
        case SYS_READFILE:
            ret = sys_readfile((const char*)regs->ebx, (char*)regs->ecx, regs->edx);
            break;
        case SYS_WRITEFILE:
            ret = sys_writefile((const char*)regs->ebx, (const char*)regs->ecx, regs->edx);
            break;
        case SYS_FORK:
        {
            // Parent gets the child's pid; the child is built to return
            // 0 from its own copy of this frame.
            process_t* child = process_fork(regs);
            ret = child ? (int)child->pid : -1;
            break;
        }
        case SYS_EXEC:
            // ebx = path, ecx = argv (user char**, or 0 for no args)
            ret = sys_exec((const char*)regs->ebx, (const char**)regs->ecx);
            break;
        case SYS_WAIT:
            // ebx = user int* for the exit code (or 0); waits for any child
            ret = sys_wait(0, (int*)regs->ebx);
            break;
        case SYS_WAITPID:
            // ebx = child pid, ecx = user int* for the exit code (or 0)
            ret = sys_wait(regs->ebx, (int*)regs->ecx);
            break;
        case SYS_OPEN:
            ret = sys_open((const char*)regs->ebx, regs->ecx);
            break;
        case SYS_CLOSE:
            ret = sys_close((int)regs->ebx);
            break;
        case SYS_READ_FD:
            ret = sys_read_fd((int)regs->ebx, (char*)regs->ecx, regs->edx);
            break;
        case SYS_WRITE_FD:
            ret = sys_write_fd((int)regs->ebx, (const char*)regs->ecx, regs->edx);
            break;
        case SYS_DUP2:
            ret = sys_dup2((int)regs->ebx, (int)regs->ecx);
            break;
        case SYS_GIVE_FOREGROUND:
            ret = sys_give_foreground(regs->ebx);
            break;
        case SYS_PIPE:
            ret = sys_pipe((int*)regs->ebx);
            break;
        default:
            // Unknown syscall
            ret = -1;
            break;
    }

    // Return value goes back in eax (regs points into the saved
    // interrupt frame, so popa restores this into the caller's eax)
    regs->eax = (uint32_t)ret;
}

void syscall_init(void)
{
    // IDT entry is registered in idt_init() via syscall_stub
    // No additional initialization needed for now
}
