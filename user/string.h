// Userland string/mem functions for GordOS user programs (Phase 1.4).
// Pure computation, no syscalls needed, self-contained like every
// other userland file until the shared libc consolidation.
#ifndef STRING_H
#define STRING_H

#include <stddef.h>

size_t strlen(const char* str);
int    strcmp(const char* a, const char* b);
int    strncmp(const char* a, const char* b, size_t n);
char*  strcpy(char* dst, const char* src);
char*  strncpy(char* dst, const char* src, size_t n);
char*  strchr(const char* s, int c);
char*  strrchr(const char* s, int c);

// GCC can emit implicit calls to these even in freestanding mode
// (struct copies, array initialization), so they need to exist for
// any userland program compiled with this file linked in.
void* memcpy(void* dst, const void* src, size_t n);
void* memset(void* dst, int value, size_t n);
void* memmove(void* dst, const void* src, size_t n);

#endif