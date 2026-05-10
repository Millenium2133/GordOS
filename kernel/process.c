#include "process.h"
#include "kmalloc.h"
#include "paging.h"

process_t* current_process = 0;

static uint32_t next_pid = 1;

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