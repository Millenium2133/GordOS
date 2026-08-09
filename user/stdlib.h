// Userland stdlib for GordOS user programs. malloc/free are backed
// by sys_sbrk (Phase 1.2); this is the file that also grows realloc,
// calloc, exit, abort, atoi, atof, strtol/strtoul, getenv, and qsort
// as GordOS's libc.a takes shape for the TCC port (Phase 2). Named
// stdlib.c/.h to match what any C program, including TCC itself,
// actually #includes for this functionality.
#ifndef STDLIB_H
#define STDLIB_H

#include <stddef.h>

void* malloc(size_t size);
void  free(void* ptr);
void* realloc(void* ptr, size_t new_size);
void* calloc(size_t nmemb, size_t size);

void  exit(int status);
void  abort(void);

int  atoi(const char* s);
long strtol(const char* nptr, char** endptr, int base);
unsigned long strtoul(const char* nptr, char** endptr, int base);

char* getenv(const char* name);

void qsort(void* base, size_t nmemb, size_t size,
           int (*compar)(const void*, const void*));

#endif // STDLIB_H