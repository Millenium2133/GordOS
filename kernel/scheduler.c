#include "scheduler.h"
#include "process.h"
#include "paging.h"
#include "gdt.h"

// Circular linked list of ready processes
static process_t* ready_queue = 0;
static uint32_t tick_count = 0;

#define TIMESLICE 10  // switch every 10 PIT ticks (~10ms at 1000Hz)

void scheduler_init(void)
{
    ready_queue  = 0;
    tick_count   = 0;
}

void scheduler_add(process_t* proc)
{
    if (!proc)
        return;

    proc->state = PROCESS_READY;

    if (!ready_queue)
    {
        // First process — point to itself
        proc->next  = proc;
        ready_queue = proc;
    }
    else
    {
        // Insert after current head
        proc->next        = ready_queue->next;
        ready_queue->next = proc;
    }
}

void scheduler_remove(process_t* proc)
{
    if (!proc || !ready_queue)
        return;

    // Find the process before this one in the circular list
    process_t* prev = ready_queue;
    while (prev->next != proc && prev->next != ready_queue)
        prev = prev->next;

    if (prev->next != proc)
        return; // not found

    prev->next = proc->next;

    if (ready_queue == proc)
        ready_queue = proc->next == proc ? 0 : proc->next;

    proc->next = 0;
}

// Called from the PIT handler every tick
void scheduler_tick(void)
{
    if (!ready_queue || !current_process)
        return;

    tick_count++;
    if (tick_count >= TIMESLICE)
    {
        tick_count = 0;
        scheduler_switch();
    }
}
// Perform a context switch to the next ready process.
// This is called either from scheduler_tick (preemptive)
// or directly (voluntary yield).
void scheduler_switch(void)
{
    if (!ready_queue)
        return;

    process_t* next = ready_queue->next;

    if (next == current_process)
        return; // only one process, nothing to switch to

    process_t* prev = current_process;
    current_process = next;
    ready_queue     = next;

    // Update TSS so syscalls from the new process use its kernel stack
    tss_set_kernel_stack(next->kernel_stack_top);

    // Switch address space
    paging_switch_address_space(next->page_directory);

    // The actual register save/restore is done in assembly.
    extern void scheduler_asm_switch(uint32_t* old_esp, uint32_t new_esp);
    scheduler_asm_switch(&prev->esp, next->esp);
}