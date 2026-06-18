#include "syscall.h"
#include "vga.h"
#include "process.h"
#include "scheduler.h"
#include "keyboard.h"
#include "pit.h"
#include "fat32.h"
#include "kmalloc.h"

void terminal_writestring(const char* data);
void terminal_putchar(char c);
void terminal_backspace(void);

// Check that [buf, buf+len) lies entirely in user space
static int user_range_ok(const void* buf, uint32_t len)
{
    uint32_t start = (uint32_t)buf;
    return buf && start < 0xC0000000 && len <= 0xC0000000 - start;
}

// sys_write: write ebx (buffer) of ecx (length) bytes to the terminal
static int sys_write(const char* buf, uint32_t len)
{
    // User code must not be able to make the kernel read its own
    // address space
    if (!user_range_ok(buf, len))
        return -1;

    uint32_t i;
    for (i = 0; i < len; i++)
    {
      terminal_putchar(buf[i]);
    }
    return (int)len;
}

// sys_read: read up to ecx bytes of keyboard input into ebx.
// Blocks until at least one byte is available, then returns whatever is
// buffered. Consumed characters are echoed to the terminal.
//
// We run in syscall context with interrupts disabled, so draining the
// ring buffer and then deciding to block is atomic against the keyboard
// IRQ — no lost wakeup. When there's nothing to read we sleep on the
// scheduler (BLOCK_READ) instead of spinning; the keyboard handler wakes
// us when it delivers a character.
static int sys_read(char* buf, uint32_t len)
{
    if (!user_range_ok(buf, len))
        return -1;
    if (len == 0)
        return 0;

    uint32_t got = 0;

    for (;;)
    {
        int c;
        while (got < len && (c = keyboard_read_char()) >= 0)
        {
            buf[got++] = (char)c;

            // Echo so the user sees what they're typing
            if (c == '\b')
                terminal_backspace();
            else
                terminal_putchar((char)c);
        }

        if (got > 0)
            break;

        // Nothing buffered — block until the keyboard delivers input.
        scheduler_block(BLOCK_READ, 0);
        scheduler_switch();
    }

    return (int)got;
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

// sys_open: open the file named by ebx (flags in ecx are reserved and
// currently ignored — read-only). Returns a small fd index, or -1 if
// the file isn't found or the process has no free fd slot.
static int sys_open(const char* upath, uint32_t flags)
{
    (void)flags;

    char path[256];
    if (copy_user_path(upath, path, sizeof(path)) != 0)
        return -1;

    uint32_t first_cluster = 0, size = 0;
    if (fat32_lookup_file(path, &first_cluster, &size) != 0)
        return -1;

    process_t* me = current_process;
    if (!me)
        return -1;

    for (int fd = 0; fd < MAX_FDS; fd++)
    {
        if (!me->fds[fd].in_use)
        {
            me->fds[fd].in_use         = 1;
            me->fds[fd].first_cluster  = first_cluster;
            me->fds[fd].cur_cluster    = first_cluster;
            me->fds[fd].cluster_offset = 0;
            me->fds[fd].pos            = 0;
            me->fds[fd].size           = size;
            return fd;
        }
    }
    return -1; // no free slot
}

// sys_read_fd: read up to len bytes from an open fd into a user buffer,
// resuming from the fd's cached cluster position. Returns the number of
// bytes read (0 at EOF), or -1 on a bad fd/buffer.
static int sys_read_fd(int fd, char* buf, uint32_t len)
{
    process_t* me = current_process;
    if (!me || fd < 0 || fd >= MAX_FDS || !me->fds[fd].in_use)
        return -1;
    if (!user_range_ok(buf, len))
        return -1;
    if (len == 0)
        return 0;

    file_desc_t* f = &me->fds[fd];
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

        uint32_t avail = cbytes - f->cluster_offset;       // left in this cluster
        uint32_t left  = f->size - f->pos;                 // left in the file
        uint32_t want  = len - got;                        // left to satisfy
        uint32_t n = want;
        if (n > avail) n = avail;
        if (n > left)  n = left;

        for (uint32_t i = 0; i < n; i++)
            buf[got + i] = (char)cluster_buf[f->cluster_offset + i];

        got              += n;
        f->pos           += n;
        f->cluster_offset += n;

        // Step to the next cluster once this one is fully consumed
        if (f->cluster_offset >= cbytes)
        {
            f->cur_cluster    = fat32_next_cluster(f->cur_cluster);
            f->cluster_offset = 0;
        }
    }

    kfree(cluster_buf);
    return (int)got;
}

// sys_write_fd: incremental writes through an fd aren't supported — that
// would mean growing files cluster-by-cluster and rewriting directory
// sizes mid-stream. Use sys_writefile for whole-file writes. Returns -1.
static int sys_write_fd(int fd, const char* buf, uint32_t len)
{
    (void)fd; (void)buf; (void)len;
    return -1;
}

// sys_close: release an fd. Read fds hold no kernel resources, so this
// just marks the slot free. Returns 0, or -1 on a bad fd.
static int sys_close(int fd)
{
    process_t* me = current_process;
    if (!me || fd < 0 || fd >= MAX_FDS || !me->fds[fd].in_use)
        return -1;
    me->fds[fd].in_use = 0;
    return 0;
}

// sys_exec: replace the calling program with the one named by ebx.
// Reads the file into a kernel buffer, then hands it (and ownership)
// to process_exec, which enters the new program and never returns on
// success. Returns -1 only if the file is missing or not a valid ELF.
static int sys_exec(const char* upath)
{
    char path[256];
    if (copy_user_path(upath, path, sizeof(path)) != 0)
        return -1;

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
    return process_exec(current_process, buf, size);
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
            ret = sys_exec((const char*)regs->ebx);
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
