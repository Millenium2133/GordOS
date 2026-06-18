#ifndef WBUF_H
#define WBUF_H

#include <stdint.h>

// A refcounted, in-memory write buffer behind a writable fd. Writes
// accumulate here and are flushed to disk (via the whole-file FAT path)
// when the last reference is closed — so dup2 and fork can share a
// redirected stdout without double-writing or losing data, and we never
// touch the FAT on-disk format incrementally.
typedef struct wbuf
{
    char*    buf;
    uint32_t len;
    int      refcount;
    char     path[64];
} wbuf_t;

// Open a write buffer for path (truncating semantics: flushed contents
// replace the file). Returns a buffer with refcount 1, or 0 on failure.
wbuf_t* wbuf_open(const char* path);

// Add/drop a reference. wbuf_unref flushes to disk and frees when the
// last reference goes away.
void wbuf_ref(wbuf_t* w);
void wbuf_unref(wbuf_t* w);

// Append up to len bytes; returns the number accepted (may be short if
// the buffer is full), or -1 on a null buffer.
int wbuf_write(wbuf_t* w, const void* data, uint32_t len);

#endif
