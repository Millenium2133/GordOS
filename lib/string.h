#ifndef STRING_H
#define STRING_H

#include <stddef.h>

size_t strlen(const char* str);
int strcmp(const char* a, const char* b);
int strncmp(const char* a, const char* b, size_t n);

// GCC may emit calls to these even in freestanding mode,
// so a kernel must always provide them.
void* memcpy(void* dst, const void* src, size_t n);
void* memset(void* dst, int value, size_t n);

#endif
