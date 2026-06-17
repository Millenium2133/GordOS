#include "syscall.h"
#include "vga.h"
#include "process.h"
#include "keyboard.h"
#include "pit.h"
#include "fat32.h"

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
// Blocks until at least one byte is available, then returns whatever
// is buffered. Consumed characters are echoed to the terminal.
static int sys_read(char* buf, uint32_t len)
{
    if (!user_range_ok(buf, len))
        return -1;
    if (len == 0)
        return 0;

    uint32_t got = 0;

    // The int 0x80 gate cleared IF; re-enable interrupts so the
    // keyboard IRQ can fill the buffer while we wait
    asm volatile("sti");

    while (got == 0)
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

        if (got == 0)
            asm volatile("hlt");
    }

    asm volatile("cli");
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

// sys_exit: tear down the calling process. The scheduler switches to
// another task and the kernel task's reaper frees this one's memory.
static void sys_exit(int code)
{
    (void)code;

    process_exit(); // never returns for a real process

    // Only reached if somehow called outside a process context
    asm volatile("sti");
    for (;;)
        asm volatile("hlt");
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
