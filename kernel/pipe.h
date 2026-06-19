#ifndef PIPE_H
#define PIPE_H

#include <stdint.h>

#define PIPE_BUF_SIZE 4096

struct process;

typedef struct pipe {
    char buf[PIPE_BUF_SIZE];
    int  head;
    int  tail;
    int  read_open;
    int  write_open;
    struct process* blocked_reader;
    struct process* blocked_writer;
} pipe_t;

pipe_t* pipe_create(void);
void pipe_destroy(pipe_t* p);
void pipe_close_read(pipe_t* p);
void pipe_close_write(pipe_t* p);

#endif
