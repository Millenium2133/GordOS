#include "process.h"
#include "kmalloc.h"
#include "paging.h"
#include "gdt.h"
#include "scheduler.h"
#include "shell.h"

// Defined in usermode.s: loads user segments and irets to ring 3
// using the frame process_start() builds on the new kernel stack
extern void process_bootstrap(void);

// From boot.s
extern uint32_t boot_page_directory[1024];
extern uint32_t stack_top;

process_t* current_process    = 0;
process_t* foreground_process = 0;

// The boot flow (kernel_main's idle loop + the shell running from the
// keyboard IRQ) is itself schedulable, as pid 0
static process_t kernel_task;

// Dead processes waiting to be freed by the kernel task
static process_t* zombie_list = 0;

static uint32_t next_pid = 1;

void process_init(void)
{
    kernel_task.pid              = 0;
    kernel_task.state            = PROCESS_RUNNING;
    kernel_task.esp              = 0;
    kernel_task.page_directory   = boot_page_directory;
    kernel_task.kernel_stack     = 0; // boot stack lives in .bss
    kernel_task.kernel_stack_top = (uint32_t)&stack_top;
    kernel_task.foreground       = 0;
    kernel_task.next             = 0;

    current_process    = &kernel_task;
    foreground_process = 0;
    zombie_list        = 0;

    scheduler_add(&kernel_task);
    kernel_task.state = PROCESS_RUNNING;
}

process_t* process_create(void)
{
    process_t* proc = kmalloc(sizeof(process_t));
    if (!proc)
        return 0;

    // Allocate kernel stack
    proc->kernel_stack = kmalloc(KERNEL_STACK_SIZE);
    if (!proc->kernel_stack)
    {
        kfree(proc);
        return 0;
    }

    proc->kernel_stack_top = (uint32_t)(proc->kernel_stack + KERNEL_STACK_SIZE);

    // Create a new address space for this process
    proc->page_directory = paging_create_address_space();
    if (!proc->page_directory)
    {
        kfree(proc->kernel_stack);
        kfree(proc);
        return 0;
    }

    proc->pid        = next_pid++;
    proc->state      = PROCESS_READY;
    proc->esp        = 0;
    proc->foreground = 0;
    proc->next       = 0;

    return proc;
}

void process_start(process_t* proc, uint32_t entry, uint32_t user_esp, int foreground)
{
    if (!proc)
        return;

    // Build the initial kernel stack: an iret frame to ring 3, a
    // return address into process_bootstrap, and four zeroed
    // callee-saved registers for scheduler_asm_switch to pop.
    uint32_t* sp = (uint32_t*)proc->kernel_stack_top;

    *--sp = 0x23;                         // ss  (user data, RPL 3)
    *--sp = user_esp;                     // esp
    *--sp = 0x202;                        // eflags (IF set)
    *--sp = 0x1B;                         // cs  (user code, RPL 3)
    *--sp = entry;                        // eip
    *--sp = (uint32_t)process_bootstrap;  // scheduler_asm_switch rets here
    *--sp = 0;                            // ebp
    *--sp = 0;                            // edi
    *--sp = 0;                            // esi
    *--sp = 0;                            // ebx

    proc->esp        = (uint32_t)sp;
    proc->foreground = foreground;

    if (foreground)
        foreground_process = proc;

    scheduler_add(proc);
}

void process_exit(void)
{
    process_t* proc = current_process;

    // The kernel task can never exit
    if (!proc || proc == &kernel_task)
        return;

    if (foreground_process == proc)
        foreground_process = 0;

    // Mark dead and switch away. scheduler_switch unlinks a dead
    // outgoing process and hands it to the reaper, so we never run
    // again — the kernel stack we are standing on stays valid only
    // until the kernel task's reaper frees it.
    proc->state = PROCESS_DEAD;
    scheduler_switch();

    // Unreachable
    for (;;)
        asm volatile("hlt");
}

void process_zombie_add(process_t* proc)
{
    proc->next  = zombie_list;
    zombie_list = proc;
}

void process_reap(void)
{
    // The zombie list is also touched from interrupt/syscall contexts
    asm volatile("cli");
    process_t* list = zombie_list;
    zombie_list = 0;
    asm volatile("sti");

    while (list)
    {
        process_t* proc = list;
        list = list->next;

        uint32_t pid = proc->pid;
        int fg       = proc->foreground;

        asm volatile("cli");
        process_destroy(proc);
        asm volatile("sti");

        shell_on_process_exit(pid, fg);
    }
}

void process_destroy(process_t* proc)
{
    if (!proc || proc == &kernel_task)
        return;

    if (proc->page_directory)
        paging_destroy_address_space(proc->page_directory);

    if (proc->kernel_stack)
        kfree(proc->kernel_stack);

    kfree(proc);
}
