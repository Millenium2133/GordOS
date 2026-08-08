// Standalone test for the userland string/mem library (Phase 1.4).
// Uses crt0 (Phase 1.3) as its entry point.
//
// Build:    make user
// Install:  make disk (copies as STRINGTEST.ELF)
// Run:      exec STRINGTEST.ELF

#include "string.h"

#define SYS_WRITE 0

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

static void print(const char* s)
{
    write(s, (int)strlen(s));
}

static int failures = 0;

static void check(int condition, const char* name)
{
    print(name);
    print(condition ? "... OK\n" : "... FAIL\n");
    if (!condition)
        failures++;
}

int main(int argc, char** argv)
{
    (void)argc;
    (void)argv;

    // strlen
    check(strlen("hello") == 5, "strlen");
    check(strlen("") == 0, "strlen empty");

    // strcmp / strncmp
    check(strcmp("abc", "abc") == 0, "strcmp equal");
    check(strcmp("abc", "abd") < 0, "strcmp less");
    check(strcmp("abd", "abc") > 0, "strcmp greater");
    check(strncmp("abcxyz", "abcqrs", 3) == 0, "strncmp equal prefix");
    check(strncmp("abcxyz", "abdqrs", 3) != 0, "strncmp differing prefix");

    // strcpy / strncpy
    char buf1[16];
    strcpy(buf1, "copied");
    check(strcmp(buf1, "copied") == 0, "strcpy");

    char buf2[8];
    for (int i = 0; i < 8; i++) buf2[i] = 'X'; // sentinel fill
    strncpy(buf2, "hi", 8);
    check(buf2[0] == 'h' && buf2[1] == 'i' && buf2[2] == '\0' && buf2[7] == '\0',
          "strncpy pads with NULs");

    // strchr / strrchr
    const char* s = "hello world";
    check(strchr(s, 'o') == s + 4, "strchr first o");
    check(strrchr(s, 'o') == s + 7, "strrchr last o");
    check(strchr(s, 'z') == 0, "strchr missing char returns NULL");
    check(*strchr(s, '\0') == '\0', "strchr NUL terminator");

    // memcpy
    char src[5] = { 1, 2, 3, 4, 5 };
    char dst[5] = { 0 };
    memcpy(dst, src, 5);
    check(dst[0] == 1 && dst[4] == 5, "memcpy");

    // memset
    char blk[6];
    memset(blk, 0x7A, 6);
    int memset_ok = 1;
    for (int i = 0; i < 6; i++)
        if (blk[i] != 0x7A) memset_ok = 0;
    check(memset_ok, "memset");

    // memmove, non-overlapping (should behave like memcpy)
    char mv_src[4] = { 10, 20, 30, 40 };
    char mv_dst[4] = { 0 };
    memmove(mv_dst, mv_src, 4);
    check(mv_dst[0] == 10 && mv_dst[3] == 40, "memmove non-overlapping");

    // memmove, overlapping forward (dst < src) - shift left
    char overlap_fwd[8] = { 1, 2, 3, 4, 5, 6, 7, 8 };
    memmove(overlap_fwd, overlap_fwd + 2, 6); // shift left by 2
    check(overlap_fwd[0] == 3 && overlap_fwd[5] == 8, "memmove overlap dst<src");

    // memmove, overlapping backward (dst > src) - shift right
    char overlap_bwd[8] = { 1, 2, 3, 4, 5, 6, 7, 8 };
    memmove(overlap_bwd + 2, overlap_bwd, 6); // shift right by 2
    check(overlap_bwd[2] == 1 && overlap_bwd[7] == 6, "memmove overlap dst>src");

    if (failures == 0)
        print("All string tests passed.\n");
    else
        print("Some string tests FAILED.\n");

    return failures;
}