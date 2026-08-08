// Standalone test for stdio (Phase 1.5, last item in Phase 1
// groundwork). Exercises printf's format specifiers, and a real
// fopen/fwrite/fclose + fopen/fread/fclose round trip against the
// disk, the first Phase 1 test that touches the filesystem rather
// than just memory.
//
// Build:    make user
// Install:  make disk (copies as STDIOTEST.ELF)
// Run:      exec STDIOTEST.ELF

#include "stdio.h"
#include "string.h"

static int failures = 0;

static void check(int condition, const char* name)
{
    printf("%s... %s\n", name, condition ? "OK" : "FAIL");
    if (!condition)
        failures++;
}

int main(int argc, char** argv)
{
    (void)argc;
    (void)argv;

    // Exercise every format specifier printf supports. These are
    // eyeballed against the printed output (there's no snprintf here
    // to diff against on real hardware), but the same conversion
    // logic was already checked byte-for-byte against the host's
    // real printf before this was written.
    printf("--- printf format specifier check ---\n");
    printf("%%d: %d and %d\n", 42, -42);
    printf("%%u: %u\n", 4000000000u);
    printf("%%x / %%X: %x / %X\n", 0xDEADBEEF, 0xDEADBEEF);
    printf("%%c: %c%c%c\n", 'a', 'b', 'c');
    printf("%%s: %s\n", "a string");
    printf("%%%%: 100%%\n");
    printf("mixed: [%d] [%s] [%x]\n", -7, "mid", 255);

    // Real file I/O round trip
    printf("\n--- file I/O round trip ---\n");

    const char* path = "STDIOT.TMP";
    const char* message = "Hello from stdiotest - written via fwrite.";
    int msg_len = (int)strlen(message);

    FILE* wf = fopen(path, "w");
    check(wf != 0, "fopen for write");

    if (wf)
    {
        size_t written = fwrite(message, 1, (size_t)msg_len, wf);
        check((int)written == msg_len, "fwrite wrote all bytes");
        check(fclose(wf) == 0, "fclose after write");
    }

    char readback[128];
    for (int i = 0; i < 128; i++) readback[i] = 0; // sentinel

    FILE* rf = fopen(path, "r");
    check(rf != 0, "fopen for read");

    if (rf)
    {
        size_t got = fread(readback, 1, (size_t)msg_len, rf);
        check((int)got == msg_len, "fread got all bytes");
        check(strncmp(readback, message, (size_t)msg_len) == 0, "readback matches what was written");
        check(fclose(rf) == 0, "fclose after read");
    }

    printf("\n");
    if (failures == 0)
        printf("All stdio tests passed.\n");
    else
        printf("Some stdio tests FAILED (%d).\n", failures);

    return failures;
}