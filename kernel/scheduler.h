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

// Call fn for every process currently in the ready queue
void scheduler_for_each(void (*fn)(process_t*));

#endif