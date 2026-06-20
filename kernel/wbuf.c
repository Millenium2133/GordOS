#include "wbuf.h"
#include "kmalloc.h"
#include "vfs.h"
#include "string.h"

// Cap on a single redirected output. Plenty for program output; writes
// past it are dropped (wbuf_write returns a short count).
#define WBUF_CAP 65536

wbuf_t* wbuf_open(const char* path)
{
    wbuf_t* w = kmalloc(sizeof(wbuf_t));
    if (!w)
        return 0;

    w->buf = kmalloc(WBUF_CAP);
    if (!w->buf)
    {
        kfree(w);
        return 0;
    }

    w->len      = 0;
    w->refcount = 1;

    int i = 0;
    while (path[i] && i < (int)sizeof(w->path) - 1)
    {
        w->path[i] = path[i];
        i++;
    }
    w->path[i] = '\0';

    return w;
}

void wbuf_ref(wbuf_t* w)
{
    if (w)
        w->refcount++;
}

void wbuf_unref(wbuf_t* w)
{
    if (!w)
        return;
    if (--w->refcount > 0)
        return;

    // Last reference: commit the accumulated bytes to disk, then free.
    vfs_write_file(w->path, w->buf, w->len);
    kfree(w->buf);
    kfree(w);
}

int wbuf_write(wbuf_t* w, const void* data, uint32_t len)
{
    if (!w)
        return -1;

    uint32_t space = WBUF_CAP - w->len;
    if (len > space)
        len = space;

    memcpy(w->buf + w->len, data, len);
    w->len += len;
    return (int)len;
}
