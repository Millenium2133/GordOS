// Minimal userland stdio for GordOS user programs (Phase 1.5, the
// last item in Phase 1 groundwork). printf is backed bt sys_write.
// fopen/fclose/fread/fwrite are backed by the existing fd syscalls
// (sys_open, sys_close, sys_read_fd, sys_write_fd). This is what TCC
// needs to read source files and write its output, without it, TCC
// cannot compile anything.
#ifndef STDIO_H
#define STDIO_H

#include <stddef.h>

// opaque handle, real definition (and the fixed-size stream table
// backing it) lives in stdio.c. No malloc dependency: a small static
// pool of streams keep stdio useable standalone, without pulling
// malloc.c into every program that just wants printf.
typedef struct FILE FILE;

FILE* fopen(const char* path, const char* mode); // mode: "r" or "w" (leading char only matters)
int fclose(FILE* stream);
size_t fread(void* ptr, size_t size, size_t nmemb, FILE* stream);
size_t fwrite(const void* ptr, size_t size, size_t nmemb, FILE* stream);

// Minimal printf: %d %u %x %X %c %s %% . No width/precision/length
// modifiers, "minimal" per the README, TCC's own usage can be
// widened later if it turns out to need more.
int printf(const char* fmt, ...);

#endif