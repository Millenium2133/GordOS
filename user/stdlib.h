// Minimal userland malloc/free for GordOS user programs, backed by
// sys_sbrk. Standalone test bed for Phase 1.2 of the groundwork.
// Not yet wired into a shared libc (that's a later consolidation step).
#ifndef MALLOC_H
#define MALLOC_H

#include <stddef.h>

void* malloc(size_t size);
void free(void* ptr);


#endif