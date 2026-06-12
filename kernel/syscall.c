#include "syscall.h"
#include "vga.h"
#include "process.h"
#include "keyboard.h"
#include "pit.h"

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

// sys_exit: tear down the calling process and hand control back to
// whoever started it (the shell's exec command)
static void sys_exit(int code)
{
    (void)code;

    if (current_process)
        process_exit(); // never returns

    // No process context (exit called from the kernel itself):
    // park with interrupts on so the shell keeps running
    terminal_writestring("Process exited\n");
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
