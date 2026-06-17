#ifndef PROCESS_H
#define PROCESS_H

#include <stdint.h>

#define PROCESS_READY   0
#define PROCESS_RUNNING 1
#define PROCESS_BLOCKED 2
#define PROCESS_DEAD    3

// Per-process kernel stack. Interrupts and syscalls from ring 3 run on
// this (via TSS esp0), including the whole shell from the keyboard IRQ,
// so it needs headroom beyond a single page.
#define KERNEL_STACK_SIZE 16384

typedef struct process
{
    uint32_t pid;
    uint32_t state;

    // Saved kernel-side stack pointer (valid while not running)
    uint32_t esp;

    // This process's page directory (virtual address)
    uint32_t* page_directory;

    // Kernel stack for this process (used when handling syscalls/interrupts)
    uint8_t*  kernel_stack;
    uint32_t  kernel_stack_top;

    // 1 if this process owns the keyboard (started by exec, not bg)
    int foreground;

    // Linked list (scheduler ready queue, then zombie list)
    struct process* next;
} process_t;

// Initialise the process subsystem: registers the boot flow as the
// kernel task (pid 0) and adds it to the scheduler
void process_init(void);

// Create a new process with its own address space
// Returns the new process, or 0 on failure
process_t* process_create(void);

// Build the initial kernel stack so the scheduler can switch into this
// process (it will enter ring 3 at entry with the given user stack),
// then add it to the ready queue.
void process_start(process_t* proc, uint32_t entry, uint32_t user_esp, int foreground);

// Terminate the current process: remove it from the scheduler, queue
// it for reaping, and switch away. Never returns.
void process_exit(void);

// Free zombie processes. Called from the kernel task's idle loop —
// a process can't free its own kernel stack.
void process_reap(void);

// Used by the scheduler when it switches away from a dead process
void process_zombie_add(process_t* proc);

// Free a process and all its resources (must not be running)
void process_destroy(process_t* proc);

// The currently running process (always set; pid 0 = kernel task)
extern process_t* current_process;

// The process that owns keyboard input, or 0 if the shell does
extern process_t* foreground_process;

#endif
