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

    // Saved kernel-side stack pointer
    uint32_t esp;

    // Physical address of this process's page directory
    uint32_t* page_directory;

    // Kernel stack for this process (used when handling syscalls/interrupts)
    uint8_t*  kernel_stack;
    uint32_t  kernel_stack_top;

    // Linked list
    struct process* next;
} process_t;

// Initialise the process subsystem
void process_init(void);

// Create a new process with its own address space
// Returns the new process, or 0 on failure
process_t* process_create(void);

// Free a process and all its resources
void process_destroy(process_t* proc);

// Run a process in ring 3 at the given entry point and user stack.
// Blocks until the process exits via sys_exit, then returns to the
// caller with the kernel address space restored.
void process_run(process_t* proc, uint32_t entry, uint32_t user_esp);

// Called from sys_exit: tear down the current process context and
// resume whoever called process_run. Never returns.
void process_exit(void);

// The currently running process
extern process_t* current_process;

#endif