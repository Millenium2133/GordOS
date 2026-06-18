#include "process.h"
#include "kmalloc.h"
#include "paging.h"
#include "pmm.h"
#include "gdt.h"
#include "scheduler.h"
#include "shell.h"
#include "idt.h"
#include "elf.h"
#include "usermode.h"
#include "wbuf.h"

// Set up a fresh fd table: standard streams open, everything else free.
static void fd_init_std(file_desc_t* fds)
{
    for (int i = 0; i < MAX_FDS; i++)
    {
        fds[i].kind     = FD_NONE;
        fds[i].writable = 0;
        fds[i].wbuf     = 0;
    }
    fds[0].kind = FD_TTY_IN;   // stdin
    fds[1].kind = FD_TTY_OUT;  // stdout
    fds[2].kind = FD_TTY_OUT;  // stderr
}

// Drop this process's references to any writable fds (flushing on the
// last reference). Called when a process exits or is torn down.
static void fd_close_writable(process_t* proc)
{
    for (int i = 0; i < MAX_FDS; i++)
    {
        if (proc->fds[i].kind == FD_FILE && proc->fds[i].writable && proc->fds[i].wbuf)
        {
            wbuf_unref(proc->fds[i].wbuf);
            proc->fds[i].wbuf = 0;
            proc->fds[i].kind = FD_NONE;
        }
    }
}

// Defined in usermode.s: loads user segments and irets to ring 3
// using the frame process_start() builds on the new kernel stack
extern void process_bootstrap(void);

// Defined in usermode.s: resumes a forked child from a copied int 0x80
// register frame (the tail of syscall_common)
extern void fork_child_return(void);

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

// Exit records: when a process exits we note its pid + code here, keyed
// by parent pid, so the parent's wait() can collect it whether or not
// the parent was already blocked. This outlives the process_t (which
// the reaper frees), so wait() never races the struct's lifetime.
#define MAX_EXIT_RECORDS 16
static struct {
    uint32_t pid;
    uint32_t parent_pid;
    int      code;
    int      valid;
} exit_records[MAX_EXIT_RECORDS];

static uint32_t next_pid = 1;

// Remember a child's exit for its parent to wait() on. Interrupts off.
static void record_exit(uint32_t pid, uint32_t parent_pid, int code)
{
    for (int i = 0; i < MAX_EXIT_RECORDS; i++)
    {
        if (!exit_records[i].valid)
        {
            exit_records[i].pid        = pid;
            exit_records[i].parent_pid = parent_pid;
            exit_records[i].code       = code;
            exit_records[i].valid      = 1;
            return;
        }
    }
    // Table full (a parent forked many children and never waited).
    // Overwrite the oldest slot — best effort; the table is only this
    // small because real wait()ers drain it promptly.
    exit_records[0].pid = pid;
    exit_records[0].parent_pid = parent_pid;
    exit_records[0].code = code;
    exit_records[0].valid = 1;
}

// Collect a matching exited child for wait(): parent_pid must match, and
// target_pid must match too unless it's 0 ("any child"). On success
// fills *out_pid / *out_code, clears the record, returns 0. Else -1.
// Interrupts off.
int process_reap_child(uint32_t parent_pid, uint32_t target_pid,
                       uint32_t* out_pid, int* out_code)
{
    for (int i = 0; i < MAX_EXIT_RECORDS; i++)
    {
        if (exit_records[i].valid &&
            exit_records[i].parent_pid == parent_pid &&
            (target_pid == 0 || exit_records[i].pid == target_pid))
        {
            if (out_pid)  *out_pid  = exit_records[i].pid;
            if (out_code) *out_code = exit_records[i].code;
            exit_records[i].valid = 0;
            return 0;
        }
    }
    return -1;
}

void process_init(void)
{
    kernel_task.pid              = 0;
    kernel_task.state            = PROCESS_RUNNING;
    kernel_task.esp              = 0;
    kernel_task.page_directory   = boot_page_directory;
    kernel_task.kernel_stack     = 0; // boot stack lives in .bss
    kernel_task.kernel_stack_top = (uint32_t)&stack_top;
    kernel_task.foreground       = 0;
    kernel_task.parent_pid       = 0;
    kernel_task.exit_code        = 0;
    kernel_task.block_reason     = BLOCK_NONE;
    kernel_task.wait_target      = 0;
    kernel_task.next             = 0;

    fd_init_std(kernel_task.fds);

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

    proc->pid          = next_pid++;
    proc->state        = PROCESS_READY;
    proc->esp          = 0;
    proc->foreground   = 0;
    proc->parent_pid   = current_process ? current_process->pid : 0;
    proc->exit_code    = 0;
    proc->block_reason = BLOCK_NONE;
    proc->wait_target  = 0;
    proc->next         = 0;

    fd_init_std(proc->fds);

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

// Number of dwords in a saved interrupt register frame (struct registers)
#define FRAME_DWORDS (sizeof(struct registers) / 4)

// Duplicate the calling process. Returns the child (added to the
// scheduler, ready to run and return 0 from its fork), or 0 on failure.
// Must be called from syscall context (interrupts already disabled, the
// caller's address space active). regs is the caller's saved frame.
process_t* process_fork(struct registers* regs)
{
    process_t* parent = current_process;
    if (!parent)
        return 0;

    process_t* child = process_create();   // fresh kernel stack + address space
    if (!child)
        return 0;

    child->parent_pid = parent->pid;

    // Child inherits the parent's open files (positions and all). A
    // writable fd's buffer is shared, so bump its refcount.
    for (int i = 0; i < MAX_FDS; i++)
    {
        child->fds[i] = parent->fds[i];
        if (child->fds[i].kind == FD_FILE && child->fds[i].writable)
            wbuf_ref(child->fds[i].wbuf);
    }

    // Eager full copy of the parent's user address space
    if (paging_copy_address_space(parent->page_directory, child->page_directory) != 0)
    {
        process_destroy(child);
        return 0;
    }

    // Build the child's kernel stack so scheduler_asm_switch resumes it
    // via fork_child_return: a copy of the parent's int 0x80 frame (with
    // eax = 0 so the child sees fork() return 0), then the trampoline
    // address, then four dummy callee-saved registers for the switch to
    // pop.
    uint32_t* sp = (uint32_t*)child->kernel_stack_top;

    sp -= FRAME_DWORDS;
    uint32_t* frame     = sp;
    uint32_t* src_frame = (uint32_t*)regs;
    for (uint32_t i = 0; i < FRAME_DWORDS; i++)
        frame[i] = src_frame[i];
    ((struct registers*)frame)->eax = 0;   // child's fork() returns 0

    *--sp = (uint32_t)fork_child_return;    // scheduler_asm_switch rets here
    *--sp = 0;                              // ebp
    *--sp = 0;                              // edi
    *--sp = 0;                              // esi
    *--sp = 0;                              // ebx

    child->esp = (uint32_t)sp;

    // Children start as background tasks; the parent keeps the keyboard
    child->foreground = 0;

    scheduler_add(child);
    return child;
}

int process_exec(process_t* proc, void* elf_data, uint32_t elf_size)
{
    if (!proc || !elf_data)
    {
        kfree(elf_data);
        return -1;
    }

    // Validate the ELF header BEFORE we destroy anything, so a bad image
    // leaves the caller's address space intact and exec can return -1.
    if (elf_size < sizeof(elf_header_t))
    {
        kfree(elf_data);
        return -1;
    }
    elf_header_t* header = (elf_header_t*)elf_data;
    if (*(uint32_t*)header->ident != ELF_MAGIC || header->type != ET_EXEC)
    {
        kfree(elf_data);
        return -1;
    }

    // Commit: tear down the current user mappings (keep the directory)
    // and flush the TLB by reloading our own cr3.
    paging_clear_user_space(proc->page_directory);
    paging_switch_address_space(proc->page_directory);

    // Load the new program and give it a fresh user stack.
    uint32_t entry = elf_load(proc, elf_data, elf_size);
    void* stack_phys = entry ? pmm_alloc_page() : 0;
    if (entry && stack_phys &&
        paging_map_page_in(proc->page_directory, USER_STACK_PAGE, (uint32_t)stack_phys,
                           PAGE_PRESENT | PAGE_WRITEABLE | PAGE_USER) == 0)
    {
        kfree(elf_data);
        jump_to_usermode(entry, USER_STACK_TOP); // never returns
    }

    // Out of memory after we already tore the old program down — nothing
    // left to return to, so the process dies.
    if (stack_phys)
        pmm_free_page(stack_phys);
    kfree(elf_data);
    process_exit(); // never returns
    return -1;      // unreachable
}

void process_exit(void)
{
    process_t* proc = current_process;

    // The kernel task can never exit
    if (!proc || proc == &kernel_task)
        return;

    if (foreground_process == proc)
        foreground_process = 0;

    // Flush and release any redirected output before we leave (so
    // `cmd > file` persists even if the program never closed the fd).
    fd_close_writable(proc);

    // Record the exit so the parent's wait() can collect it, and wake the
    // parent if it's already blocked in wait. Skip kernel-task children
    // (pid 0 never waits) so their records don't fill the table.
    if (proc->parent_pid != 0)
    {
        record_exit(proc->pid, proc->parent_pid, proc->exit_code);
        scheduler_wake_waiter(proc->parent_pid, proc->pid);
    }

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

        uint32_t pid    = proc->pid;
        int fg          = proc->foreground;
        uint32_t parent = proc->parent_pid;

        asm volatile("cli");
        process_destroy(proc);
        asm volatile("sti");

        // Only the kernel shell's own children (parented to the kernel
        // task, pid 0) get the "[n] done" notice + prompt redraw. A
        // child of a user process — e.g. one a user-space shell forked —
        // is reaped silently; that shell collects it with wait().
        if (parent == 0)
            shell_on_process_exit(pid, fg);
    }
}

void process_destroy(process_t* proc)
{
    if (!proc || proc == &kernel_task)
        return;

    // Release any writable fds still open (e.g. a killed process that
    // never reached process_exit). Flushes on the last reference.
    fd_close_writable(proc);

    if (proc->page_directory)
        paging_destroy_address_space(proc->page_directory);

    if (proc->kernel_stack)
        kfree(proc->kernel_stack);

    kfree(proc);
}
