#include "syscall.h"
#include "vga.h"

void terminal_writestring(const char* data);
void terminal_putchar(char c);

// sys_write: write ebx (buffer) of ecx (length) bytes to the terminal
static int sys_write(const char* buf, uint32_t len)
{
    if (!buf)
    {
        return -1; // Invalid buffer
    }

    uint32_t i;
    for (i = 0; i < len; i++)
    {      
      terminal_putchar(buf[i]);
    }
    return (int)len;
}

// sys_exit: for now just halt the CPU
static void sys_exit(int code)
{
    (void)code;
    terminal_writestring("Process exited\n");
    for (;;)
        asm volatile("hlt");
}

// sys_getpid: return a dummy PID (for now)
static int sys_getpid()
{
    return 0; 
}

void syscall_handler(struct registers regs)
{
    int ret = 0;

    switch (regs.eax)
    {
        case SYS_WRITE:
            ret = sys_write((const char*)regs.ebx, regs.ecx);
            break;
        case SYS_EXIT:
            sys_exit((int)regs.ebx);
            break;
        case SYS_GETPID:
            ret = sys_getpid();
            break;
        default:
            // Unknown syscall
            ret = -1;
            break;
    }

    // Return value goes back in eax
    (void)ret;
}

void syscall_init(void)
{
    // IDT entry is registered in idt_init() via syscall_stub
    // No additional initialization needed for now
}