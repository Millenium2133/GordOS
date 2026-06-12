#include "process.h"
#include "kmalloc.h"
#include "paging.h"
#include "gdt.h"

// Defined in usermode.s
extern void process_launch(uint32_t entry, uint32_t user_esp, uint32_t* save_esp);
extern void process_return(uint32_t saved_esp);

process_t* current_process = 0;

static uint32_t next_pid = 1;

// Kernel context of the process_run caller, restored on process exit
static uint32_t run_caller_esp = 0;

void process_init(void)
{
    current_process = 0;
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

    proc->pid   = next_pid++;
    proc->state = PROCESS_READY;
    proc->esp   = 0;
    proc->next  = 0;

    return proc;
}

void process_run(process_t* proc, uint32_t entry, uint32_t user_esp)
{
    if (!proc)
        return;

    current_process = proc;
    proc->state     = PROCESS_RUNNING;

    // Syscalls and interrupts from ring 3 run on this process's
    // kernel stack
    tss_set_kernel_stack(proc->kernel_stack_top);

    paging_switch_address_space(proc->page_directory);

    // Enters ring 3 and comes back here when process_exit() runs
    process_launch(entry, user_esp, &run_caller_esp);

    current_process = 0;
}

void process_exit(void)
{
    if (current_process)
        current_process->state = PROCESS_DEAD;

    // Back to the kernel's own address space before resuming the
    // process_run caller (the caller's heap pointers are higher-half,
    // so this is mostly hygiene)
    paging_switch_to_kernel();

    process_return(run_caller_esp);
}

void process_destroy(process_t* proc)
{
    if (!proc)
        return;

    if (proc->page_directory)
        paging_destroy_address_space(proc->page_directory);

    if (proc->kernel_stack)
        kfree(proc->kernel_stack);

    kfree(proc);
}