#ifndef PIT_H
#define PIT_H

#include <stdint.h>

void pit_init(uint32_t frequency);
uint32_t timer_ticks(void);
void timer_sleep(uint32_t ms);

#endif
