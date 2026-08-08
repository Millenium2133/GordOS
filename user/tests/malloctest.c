// Standalone test for the userland malloc/free port (Phase 1.2).
// Exercises: basic alloc+write+verify, an allocation big enough to
// force sys_sbrk to map multiple pages, and free+reuse of a block.
//
// Build:    make user
// Install:  make disk (copies as MALLOCTEST.ELF)
// Run:      exec MALLOCTEST.ELF

#include "stdlib.h"

#define SYS_WRITE 0
#define SYS_EXIT  1

static inline int syscall3(int num, int arg1, int arg2, int arg3)
{
    int ret;
    __asm__ volatile("int $0x80"
                     : "=a"(ret)
                     : "a"(num), "b"(arg1), "c"(arg2), "d"(arg3)
                     : "memory");
    return ret;
}

static void write(const char* s, int len)
{
    syscall3(SYS_WRITE, (int)s, len, 0);
}

static int str_len(const char* s)
{
    int n = 0;
    while (s[n])
        n++;
    return n;
}

static void print(const char* s)
{
    write(s, str_len(s));
}

void _start(void)
{
    int failures = 0;

    // Test 1: small allocation, write a pattern, read it back
    print("Test 1: small alloc + verify... ");
    char* a = (char*)malloc(64);
    if (!a)
    {
        print("FAIL (malloc returned 0)\n");
        failures++;
    }
    else
    {
        for (int i = 0; i < 64; i++)
            a[i] = (char)(i & 0xFF);

        int ok = 1;
        for (int i = 0; i < 64; i++)
        {
            if (a[i] != (char)(i & 0xFF))
            {
                ok = 0;
                break;
            }
        }
        print(ok ? "OK\n" : "FAIL (data mismatch)\n");
        if (!ok) failures++;
    }

    // Test 2: allocation bigger than one page, forces sys_sbrk to map
    // more than one page in new_block
    print("Test 2: multi-page alloc + verify... ");
    unsigned int big_size = 9000; // > 2 pages worth of data
    char* b = (char*)malloc(big_size);
    if (!b)
    {
        print("FAIL (malloc returned 0)\n");
        failures++;
    }
    else
    {
        for (unsigned int i = 0; i < big_size; i++)
            b[i] = (char)((i * 7) & 0xFF);

        int ok = 1;
        for (unsigned int i = 0; i < big_size; i++)
        {
            if (b[i] != (char)((i * 7) & 0xFF))
            {
                ok = 0;
                break;
            }
        }
        print(ok ? "OK\n" : "FAIL (data mismatch)\n");
        if (!ok) failures++;
    }

    // Test 3: free a block, then malloc a smaller size, the
    // allocator should reuse the freed block rather than growing the
    // heap again
    print("Test 3: free + reuse... ");
    if (a)
    {
        free(a);
        char* c = (char*)malloc(32);
        int ok = (c == a); // same address = reused, didn't call sbrk again
        print(ok ? "OK\n" : "FAIL (did not reuse freed block)\n");
        if (!ok) failures++;
    }
    else
    {
        print("SKIPPED (test 1 already failed)\n");
    }

    if (failures == 0)
        print("All malloc tests passed.\n");
    else
        print("Some malloc tests FAILED.\n");

    syscall3(SYS_EXIT, failures, 0, 0);

    for (;;) {}
}