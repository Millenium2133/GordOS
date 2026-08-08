// Userland stdio (Phase 1.5). Self-contained (own syscall stub) like
// every other userland file until the shared libc consolidation.
// Uses string.h's strlen for %s (Phase 1.4), the first real spot
// where two groundwork pieces compose.

#include "stdio.h"
#include "string.h"
#include <stdint.h>
#include <stdarg.h>

#define SYS_WRITE    0
#define SYS_OPEN     11
#define SYS_CLOSE    12
#define SYS_READ_FD  13
#define SYS_WRITE_FD 14

#define O_WRITE 1

static inline int syscall3(int num, int arg1, int arg2, int arg3)
{
    int ret;
    __asm__ volatile("int $0x80"
                     : "=a"(ret)
                     : "a"(num), "b"(arg1), "c"(arg2), "d"(arg3)
                     : "memory");
    return ret;
}

// --- FILE* handling -------------------------------------------------

#define MAX_STREAMS 8

struct FILE
{
    int fd;
    int in_use;
    int eof;
    int error;
};

static struct FILE stream_table[MAX_STREAMS];

static FILE* alloc_stream(void)
{
    for (int i = 0; i < MAX_STREAMS; i++)
        if (!stream_table[i].in_use)
            return &stream_table[i];
    return 0;
}

FILE* fopen(const char* path, const char* mode)
{
    int flags = (mode && mode[0] == 'w') ? O_WRITE : 0;

    int fd = syscall3(SYS_OPEN, (int)path, flags, 0);
    if (fd < 0)
        return 0;

    FILE* f = alloc_stream();
    if (!f)
    {
        syscall3(SYS_CLOSE, fd, 0, 0);
        return 0;
    }

    f->fd     = fd;
    f->in_use = 1;
    f->eof    = 0;
    f->error  = 0;
    return f;
}

int fclose(FILE* stream)
{
    if (!stream || !stream->in_use)
        return -1;

    int ret = syscall3(SYS_CLOSE, stream->fd, 0, 0);
    stream->in_use = 0;
    return ret;
}

size_t fread(void* ptr, size_t size, size_t nmemb, FILE* stream)
{
    if (!stream || !stream->in_use || size == 0 || nmemb == 0)
        return 0;

    uint32_t total = (uint32_t)(size * nmemb);
    uint32_t got = 0;
    unsigned char* buf = (unsigned char*)ptr;

    while (got < total)
    {
        int ret = syscall3(SYS_READ_FD, stream->fd, (int)(buf + got), (int)(total - got));
        if (ret == 0) { stream->eof = 1; break; }
        if (ret < 0)  { stream->error = 1; break; }
        got += (uint32_t)ret;
    }

    return got / size;
}

size_t fwrite(const void* ptr, size_t size, size_t nmemb, FILE* stream)
{
    if (!stream || !stream->in_use || size == 0 || nmemb == 0)
        return 0;

    uint32_t total = (uint32_t)(size * nmemb);
    uint32_t got = 0;
    const unsigned char* buf = (const unsigned char*)ptr;

    while (got < total)
    {
        int ret = syscall3(SYS_WRITE_FD, stream->fd, (int)(buf + got), (int)(total - got));
        if (ret < 0) { stream->error = 1; break; }
        if (ret == 0) break; // shouldn't happen, but don't spin forever
        got += (uint32_t)ret;
    }

    return got / size;
}

// --- printf -----------------------------------------------------------

static void write_out(const char* s, uint32_t len)
{
    syscall3(SYS_WRITE, (int)s, (int)len, 0);
}

// Formats val in the given base into out (not NUL-terminated),
// most-significant digit first. Returns the number of characters
// written. base must be 10 or 16.
static int format_uint(unsigned int val, int base, int uppercase, char* out)
{
    const char* digits = uppercase ? "0123456789ABCDEF" : "0123456789abcdef";

    char tmp[16];
    int len = 0;

    if (val == 0)
    {
        tmp[len++] = '0';
    }
    else
    {
        while (val > 0)
        {
            tmp[len++] = digits[val % (unsigned int)base];
            val /= (unsigned int)base;
        }
    }

    // tmp was built least-significant-digit first - reverse it into out
    for (int i = 0; i < len; i++)
        out[i] = tmp[len - 1 - i];

    return len;
}

static int format_int(int val, char* out)
{
    int len = 0;
    unsigned int uval;

    if (val < 0)
    {
        out[len++] = '-';
        // careful with INT_MIN: -val overflows, so peel one off first
        uval = (unsigned int)(-(val + 1)) + 1;
    }
    else
    {
        uval = (unsigned int)val;
    }

    len += format_uint(uval, 10, 0, out + len);
    return len;
}

int printf(const char* fmt, ...)
{
    va_list args;
    va_start(args, fmt);

    int total = 0;
    const char* p = fmt;
    const char* run_start = fmt;
    char numbuf[16];
    int numlen;

    while (*p)
    {
        if (*p != '%')
        {
            p++;
            continue;
        }

        // flush the literal text run before this '%'
        if (p > run_start)
        {
            uint32_t run_len = (uint32_t)(p - run_start);
            write_out(run_start, run_len);
            total += (int)run_len;
        }

        p++; // skip '%'

        switch (*p)
        {
            case 'd':
                numlen = format_int(va_arg(args, int), numbuf);
                write_out(numbuf, (uint32_t)numlen);
                total += numlen;
                break;

            case 'u':
                numlen = format_uint(va_arg(args, unsigned int), 10, 0, numbuf);
                write_out(numbuf, (uint32_t)numlen);
                total += numlen;
                break;

            case 'x':
                numlen = format_uint(va_arg(args, unsigned int), 16, 0, numbuf);
                write_out(numbuf, (uint32_t)numlen);
                total += numlen;
                break;

            case 'X':
                numlen = format_uint(va_arg(args, unsigned int), 16, 1, numbuf);
                write_out(numbuf, (uint32_t)numlen);
                total += numlen;
                break;

            case 'c':
            {
                char c = (char)va_arg(args, int);
                write_out(&c, 1);
                total += 1;
                break;
            }

            case 's':
            {
                const char* s = va_arg(args, const char*);
                uint32_t slen = (uint32_t)strlen(s);
                write_out(s, slen);
                total += (int)slen;
                break;
            }

            case '%':
            {
                char pc = '%';
                write_out(&pc, 1);
                total += 1;
                break;
            }

            default:
                // unknown specifier, print the '%' and whatever
                // followed it literally, rather than swallowing them
                write_out("%", 1);
                total += 1;
                if (*p)
                {
                    write_out(p, 1);
                    total += 1;
                }
                break;
        }

        if (*p)
            p++; // move past the specifier character
        run_start = p;
    }

    // flush any trailing literal run
    if (p > run_start)
    {
        uint32_t run_len = (uint32_t)(p - run_start);
        write_out(run_start, run_len);
        total += (int)run_len;
    }

    va_end(args);
    return total;
}