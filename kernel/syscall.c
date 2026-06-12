#include "syscall.h"
#include "vga.h"
#include "process.h"

void terminal_writestring(const char* data);
void terminal_putchar(char c);

// sys_write: write ebx (buffer) of ecx (length) bytes to the terminal
static int sys_write(const char* buf, uint32_t len)
{
    uint32_t start = (uint32_t)buf;

    // Reject null, kernel-space, or wrapping buffers — user code must
    // not be able to make the kernel read its own address space
    if (!buf || start >= 0xC0000000 || len > 0xC0000000 - start)
        return -1;

    uint32_t i;
    for (i = 0; i < len; i++)
    {
      terminal_putchar(buf[i]);
    }
    return (int)len;
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
