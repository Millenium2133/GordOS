#ifndef SYSCALL_H
#define SYSCALL_H

#include <stdint.h>
#include "idt.h"

#define SYS_WRITE 0
#define SYS_EXIT 1
#define SYS_GETPID 2
#define SYS_READ 3
#define SYS_SLEEP 4
#define SYS_READFILE 5
#define SYS_WRITEFILE 6
#define SYS_FORK 7
#define SYS_EXEC 8
#define SYS_WAIT 9
#define SYS_WAITPID 10
#define SYS_OPEN 11
#define SYS_CLOSE 12
#define SYS_READ_FD 13
#define SYS_WRITE_FD 14
#define SYS_DUP2            15
#define SYS_GIVE_FOREGROUND 16
#define SYS_PIPE            17
#define SYS_CHDIR           18
#define SYS_GETCWD          19
#define SYS_MKDIR           20
#define SYS_RMFILE          21
#define SYS_RENAME          22
#define SYS_LISTDIR         23
#define SYS_UPTIME          24
#define SYS_MEMINFO         25
#define SYS_KILL_PID        26
#define SYS_PS              27
#define SYS_SETCOLOR        28
#define SYS_FASTERFETCH     29
#define SYS_PETER           30
#define SYS_FINDPREFIX      31
#define SYS_READRAW         32
#define SYS_GETTIME         33
#define SYS_CLEAR           34

void syscall_init(void);
void syscall_handler(struct registers* regs);

#endif