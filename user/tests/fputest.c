// Tests the actual point of the FPU work: that two processes' FPU
// state stays independent across context switches, not just that
// floating point instructions don't fault.
//
// The parent and a forked child each run a long loop of an exact
// floating-point invariant (multiply then divide by 2.0, exact in
// IEEE754, no rounding error accumulates, so the value must return
// to precisely its starting point every iteration). The two
// processes use different starting values. If fpu_save/fpu_restore
// around a context switch were missing or wrong, one process's FPU
// registers would bleed into the other's mid-computation and a check
// would fail, this is exactly the bug fpu.c fixes.
//
// No %f in printf yet, so results are reported via an integer scale
// (value * 1000, truncated) for any diagnostic output.
//
// Build:    make user
// Install:  make disk (copies as FPUTEST.ELF)
// Run:      exec FPUTEST.ELF

#define SYS_WRITE  0
#define SYS_EXIT   1
#define SYS_FORK   7
#define SYS_WAIT   9

static inline int syscall3(int num, int a1, int a2, int a3)
{
    int ret;
    __asm__ volatile("int $0x80"
                     : "=a"(ret)
                     : "a"(num), "b"(a1), "c"(a2), "d"(a3)
                     : "memory");
    return ret;
}

static int str_len(const char* s)
{
    int n = 0;
    while (s[n]) n++;
    return n;
}

static void write_str(const char* s)
{
    syscall3(SYS_WRITE, (int)s, str_len(s), 0);
}

static void write_num(int n)
{
    char buf[16];
    int i = 0;
    if (n == 0) { buf[i++] = '0'; }
    else
    {
        int neg = n < 0;
        if (neg) n = -n;
        char tmp[16]; int t = 0;
        while (n > 0) { tmp[t++] = '0' + (n % 10); n /= 10; }
        if (neg) buf[i++] = '-';
        while (t > 0) buf[i++] = tmp[--t];
    }
    buf[i] = '\0';
    write_str(buf);
}

#define ITERATIONS 2000000

// Returns 1 if the invariant held for every iteration, 0 on first
// failure (loop stops early in that case).
static int run_invariant_check(volatile double start_value, const char* label)
{
    volatile double x = start_value;
    for (int i = 0; i < ITERATIONS; i++)
    {
        x = x * 2.0;
        x = x / 2.0;
        if (x != start_value)
        {
            write_str(label);
            write_str(": MISMATCH at iteration ");
            write_num(i);
            write_str(" - value*1000 (truncated) = ");
            write_num((int)(x * 1000.0));
            write_str(", expected ");
            write_num((int)(start_value * 1000.0));
            write_str("\n");
            return 0;
        }
    }
    write_str(label);
    write_str(": all ");
    write_num(ITERATIONS);
    write_str(" iterations held. PASS\n");
    return 1;
}

void _start(void)
{
    write_str("FPU context-switch isolation test\n");
    write_str("(two processes doing different exact FP invariants concurrently)\n\n");

    int pid = syscall3(SYS_FORK, 0, 0, 0);

    if (pid == 0)
    {
        // Child: distinct starting value from the parent's, so if
        // state leaks between them the mismatch is obviously
        // attributable to cross-contamination, not just noise.
        int ok = run_invariant_check(777.777, "child");
        syscall3(SYS_EXIT, ok ? 0 : 1, 0, 0);
        for (;;) {}
    }

    int ok = run_invariant_check(3.14159, "parent");

    int status = 0;
    syscall3(SYS_WAIT, (int)&status, 0, 0);

    write_str("\n");
    if (ok && status == 0)
        write_str("FPU isolation test: PASS (both processes' FP state stayed correct)\n");
    else
        write_str("FPU isolation test: FAIL\n");

    syscall3(SYS_EXIT, (ok && status == 0) ? 0 : 1, 0, 0);
    for (;;) {}
}