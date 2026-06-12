#ifndef SYSCALL_H
#define SYSCALL_H

#include <stdint.h>
#include "idt.h"

#define SYS_WRITE 0
#define SYS_EXIT 1
#define SYS_GETPID 2

void syscall_init(void);
void syscall_handler(struct registers* regs);

#endif