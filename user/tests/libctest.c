// Standalone test for the ctype.c and new stdlib.c functions added
// for the TCC port's libc.a (Phase 2). Uses crt0 as its entry point.
//
// Build:    make user
// Install:  make disk (copies as LIBCTEST.ELF)
// Run:      exec LIBCTEST.ELF

#include "stdio.h"
#include "stdlib.h"
#include "ctype.h"
#include "string.h"

static int failures = 0;

static void check(int condition, const char* name)
{
    printf("%s... %s\n", name, condition ? "OK" : "FAIL");
    if (!condition)
        failures++;
}

static int int_cmp(const void* a, const void* b)
{
    return *(const int*)a - *(const int*)b;
}

int main(int argc, char** argv)
{
    (void)argc;
    (void)argv;

    // ctype
    check(isalpha('a') && isalpha('Z') && !isalpha('5'), "isalpha");
    check(isdigit('7') && !isdigit('x'), "isdigit");
    check(isspace(' ') && isspace('\t') && !isspace('x'), "isspace");
    check(isxdigit('F') && isxdigit('9') && !isxdigit('g'), "isxdigit");
    check(isalnum('a') && isalnum('9') && !isalnum('!'), "isalnum");
    check(toupper('a') == 'A' && toupper('A') == 'A', "toupper");
    check(tolower('A') == 'a' && tolower('a') == 'a', "tolower");

    // atoi
    check(atoi("42") == 42, "atoi positive");
    check(atoi("-42") == -42, "atoi negative");
    check(atoi("  123") == 123, "atoi leading whitespace");
    check(atoi("abc") == 0, "atoi non-numeric returns 0");

    // strtol / strtoul
    char* end;
    check(strtol("0x1A", &end, 0) == 0x1A && *end == '\0', "strtol hex auto-base");
    check(strtol("017", 0, 0) == 15, "strtol octal auto-base");
    check(strtol("123rest", &end, 10) == 123 && strcmp(end, "rest") == 0, "strtol endptr");
    check(strtoul("0xFF", 0, 0) == 255, "strtoul hex");

    // getenv, GordOS has no env vars, NULL is the correct answer
    check(getenv("PATH") == 0, "getenv always returns NULL (no env support)");

    // qsort
    int arr[6] = { 5, 3, 8, 1, 9, 2 };
    qsort(arr, 6, sizeof(int), int_cmp);
    int sorted = 1;
    for (int i = 1; i < 6; i++)
        if (arr[i - 1] > arr[i]) sorted = 0;
    check(sorted, "qsort sorts correctly");

    int sorted_already[5] = { 1, 2, 3, 4, 5 };
    qsort(sorted_already, 5, sizeof(int), int_cmp);
    int still_sorted = 1;
    for (int i = 1; i < 5; i++)
        if (sorted_already[i - 1] > sorted_already[i]) still_sorted = 0;
    check(still_sorted, "qsort already-sorted input");

    // realloc/calloc quick smoke check (already covered in depth by
    // MALLOCTEST.ELF, just confirming they're linked and callable here)
    void* p = malloc(4);
    p = realloc(p, 64);
    check(p != 0, "realloc still works");
    free(p);
    void* c = calloc(4, 4);
    check(c != 0, "calloc still works");
    free(c);

    if (failures == 0)
        printf("\nAll libc tests passed.\n");
    else
        printf("\nSome libc tests FAILED (%d).\n", failures);

    return failures;
}