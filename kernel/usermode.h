#ifndef USERMODE_H
#define USERMODE_H

#include <stdint.h>

void jump_to_usermode(uint32_t eip, uint32_t esp);

#endif