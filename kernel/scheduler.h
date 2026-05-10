#ifndef SCHEDULER_H
#define SCHEDULER_H

#include <stdint.h>
#include "process.h"

void scheduler_init(void);
void scheduler_add(process_t* proc);
void scheduler_remove(process_t* proc);
void scheduler_tick(void);
void scheduler_switch(void);

#endif