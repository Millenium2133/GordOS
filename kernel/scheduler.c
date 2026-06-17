#include "scheduler.h"
#include "process.h"
#include "paging.h"
#include "gdt.h"

// Circular linked list of ready processes (always contains at least
// the kernel task once process_init has run)
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

process_t* scheduler_find(uint32_t pid)
{
    if (!ready_queue)
        return 0;

    process_t* p = ready_queue;
    do
    {
        if (p->pid == pid)
            return p;
        p = p->next;
    } while (p != ready_queue);

    return 0;
}

void scheduler_for_each(void (*fn)(process_t*))
{
    if (!ready_queue || !fn)
        return;

    process_t* p = ready_queue;
    do
    {
        fn(p);
        p = p->next;
    } while (p != ready_queue);
}

// Called from the PIT handler every tick
void scheduler_tick(void)
{
    if (!ready_queue || !current_process)
        return;

    tick_count++;

    // Switch at the end of the timeslice, or immediately if the
    // current process has been killed
    if (tick_count >= TIMESLICE || current_process->state == PROCESS_DEAD)
    {
        tick_count = 0;
        scheduler_switch();
    }
}

// Perform a context switch to the next ready process.
// Called from scheduler_tick (preemptive, inside the PIT IRQ) or from
// process_exit (voluntary). Interrupts are off in both contexts.
void scheduler_switch(void)
{
    if (!ready_queue || !current_process)
        return;

    process_t* prev = current_process;
    process_t* next = ready_queue->next;

    // The kernel task is always in the queue and never dies, so when
    // prev is dead there is always at least one other runnable task.
    if (next == prev)
        return; // only one process, nothing to switch to

    // Advance to the next task
    current_process = next;
    ready_queue     = next;
    next->state     = PROCESS_RUNNING;

    if (prev->state == PROCESS_DEAD)
    {
        // Unlink from the ready queue and hand to the reaper. Safe to
        // do while still standing on prev's kernel stack: we never
        // switch back to it, and the reaper (kernel task) cannot run
        // until after the switch below.
        scheduler_remove(prev);
        process_zombie_add(prev);
    }
    else
    {
        prev->state = PROCESS_READY;
    }

    // Update TSS so interrupts/syscalls from ring 3 use the new
    // process's kernel stack
    tss_set_kernel_stack(next->kernel_stack_top);

    // Switch address space
    paging_switch_address_space(next->page_directory);

    // The actual register save/restore is done in assembly.
    extern void scheduler_asm_switch(uint32_t* old_esp, uint32_t new_esp);
    scheduler_asm_switch(&prev->esp, next->esp);
}
