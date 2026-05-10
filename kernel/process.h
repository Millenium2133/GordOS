#ifndef PROCESS_H
#define PROCESS_H

#include <stdint.h>

#define PROCESS_READY   0
#define PROCESS_RUNNING 1
#define PROCESS_BLOCKED 2
#define PROCESS_DEAD    3

#define KERNEL_STACK_SIZE 4096

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

// The currently running process
extern process_t* current_process;

#endif