#include "pipe.h"
#include "kmalloc.h"

pipe_t* pipe_create(void)
{
    pipe_t* p = kmalloc(sizeof(pipe_t));
    if (!p)
        return 0;

    for (int i = 0; i < PIPE_BUF_SIZE; i++)
        p->buf[i] = 0;
    p->head           = 0;
    p->tail           = 0;
    p->read_open      = 1;
    p->write_open     = 1;
    p->blocked_reader = 0;
    p->blocked_writer = 0;
    return p;
}

void pipe_destroy(pipe_t* p)
{
    if (p)
        kfree(p);
}

void pipe_close_read(pipe_t* p)
{
    if (p)
        p->read_open--;
}

void pipe_close_write(pipe_t* p)
{
    if (p)
        p->write_open--;
}
