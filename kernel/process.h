#ifndef PROCESS_H
#define PROCESS_H

#include <stdint.h>

#define PROCESS_READY   0
#define PROCESS_RUNNING 1
#define PROCESS_BLOCKED 2
#define PROCESS_DEAD    3

// Why a PROCESS_BLOCKED process is sleeping
#define BLOCK_NONE 0
#define BLOCK_WAIT 1   // waiting for a child to exit (wait_target = pid, 0 = any)
#define BLOCK_READ 2   // waiting for keyboard input

// Per-process kernel stack. Interrupts and syscalls from ring 3 run on
// this (via TSS esp0), including the whole shell from the keyboard IRQ,
// so it needs headroom beyond a single page.
#define KERNEL_STACK_SIZE 16384

// Where a user program's stack lives (one page just below the kernel half)
#define USER_STACK_PAGE 0xBFFFF000
#define USER_STACK_TOP  0xBFFFFFF0

// Per-process open file table. An open fd caches its position in the
// FAT cluster chain so sequential reads resume without re-walking from
// the first cluster every call. Read-only for now; no kernel resources
// are held, so closing/exiting needs no cleanup. An unused slot is {0}.
#define MAX_FDS 8
typedef struct
{
    int      in_use;
    uint32_t first_cluster;
    uint32_t cur_cluster;    // cluster the next byte comes from
    uint32_t cluster_offset; // bytes already consumed within cur_cluster
    uint32_t pos;            // absolute read position, for EOF
    uint32_t size;           // total file size
} file_desc_t;

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

    // Parent/child tracking for wait()
    uint32_t parent_pid;
    int      exit_code;     // set just before the process goes DEAD

    // Why (and on what) this process is blocked, if state == BLOCKED
    int      block_reason;  // BLOCK_NONE / BLOCK_WAIT / BLOCK_READ
    uint32_t wait_target;   // for BLOCK_WAIT: child pid, or 0 for "any child"

    // Open files (fd index = slot index)
    file_desc_t fds[MAX_FDS];

    // Linked list (scheduler ready queue, blocked list, or zombie list)
    struct process* next;
} process_t;

// Initialise the process subsystem: registers the boot flow as the
// kernel task (pid 0) and adds it to the scheduler
void process_init(void);

// Create a new process with its own address space
// Returns the new process, or 0 on failure
process_t* process_create(void);

// Duplicate the calling process (fork). Returns the child, added to the
// scheduler and set to return 0 from its own fork, or 0 on failure.
// Call from syscall context with the caller's saved register frame.
struct registers;
process_t* process_fork(struct registers* regs);

// Replace the calling process's program in place (exec), reusing the
// same pid and page directory. Takes ownership of elf_data (frees it).
// On success it enters the new program in ring 3 and never returns; on
// recoverable failure (bad ELF) it returns -1 with the caller intact.
// An out-of-memory failure after teardown is unrecoverable and kills
// the process rather than returning.
int process_exec(process_t* proc, void* elf_data, uint32_t elf_size);

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

// Collect an exited child for wait(): parent_pid must match; target_pid
// must match too unless 0 ("any child"). Fills *out_pid/*out_code and
// returns 0 on success, -1 if no matching child has exited. Interrupts
// must be disabled.
int process_reap_child(uint32_t parent_pid, uint32_t target_pid,
                       uint32_t* out_pid, int* out_code);

// Used by the scheduler when it switches away from a dead process
void process_zombie_add(process_t* proc);

// Free a process and all its resources (must not be running)
void process_destroy(process_t* proc);

// The currently running process (always set; pid 0 = kernel task)
extern process_t* current_process;

// The process that owns keyboard input, or 0 if the shell does
extern process_t* foreground_process;

#endif
