// Standalone test for crt0 (Phase 1.3). Verifies argc/argv arrive in
// main() correctly, and that returning from main triggers sys_exit
// with the right code via crt0.s.
//
// Build:    make user
// Install:  make disk (copies as CRT0TEST.ELF)
// Run:      exec CRT0TEST.ELF one two three

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

int main(int argc, char** argv)
{
    print("crt0 test: main() reached, argc=");

    // No printf yet (that's 1.5), so hand-roll a decimal digit
    char digit[2] = { (char)('0' + (argc % 10)), '\0' };
    print(digit);
    print("\n");

    for (int i = 0; i < argc; i++)
    {
        print("  argv[");
        char idx[2] = { (char)('0' + (i % 10)), '\0' };
        print(idx);
        print("] = ");
        print(argv[i]);
        print("\n");
    }

    print("Returning 7 from main() to test crt0's exit path.\n");
    return 7;
}