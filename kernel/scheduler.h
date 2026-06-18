#ifndef SCHEDULER_H
#define SCHEDULER_H

#include <stdint.h>
#include "process.h"

void scheduler_init(void);
void scheduler_add(process_t* proc);
void scheduler_remove(process_t* proc);
void scheduler_tick(void);
void scheduler_switch(void);

// Look up a process in the ready queue by pid (0 if not found)
process_t* scheduler_find(uint32_t pid);

// Look up a process by pid across both the ready and blocked lists
process_t* scheduler_find_any(uint32_t pid);

// Call fn for every process currently in the ready queue
void scheduler_for_each(void (*fn)(process_t*));

// Blocking primitive. Call with interrupts disabled, matching the
// convention in scheduler_switch.
//
// scheduler_block sets the current process BLOCKED with the given
// reason/target; the caller then calls scheduler_switch(), which moves
// it off the ready queue onto the blocked list (mirroring how a DEAD
// process is handed to the reaper).
//
// scheduler_wake moves a blocked process back onto the ready queue.
void scheduler_block(int reason, uint32_t target);
void scheduler_wake(process_t* proc);

// Wake the parent blocked in wait() for an exiting child, if any.
// Returns the woken process, or 0 if nobody was waiting.
process_t* scheduler_wake_waiter(uint32_t parent_pid, uint32_t child_pid);

// Is there any live process (ready or blocked) whose parent is parent_pid?
int scheduler_has_child(uint32_t parent_pid);

#endif