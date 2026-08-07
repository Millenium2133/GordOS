// Userland string/mem functions (Phase 1.4). strlen, strcmp, strncmp,
// memcpy, and memset are ported straight from lib/string.c (They made
// no kernel-privilege assumptions to begin with). strcpy, strncpy,
// strchrm strrchr, and memmove are new, they dont exist anywhere in this codebase
// yet/

#include "string.h"

size_t strlen(const char* str)
{
    size_t len = 0;
    while (str[len])
        len++;
    return len;
}

int strcmp(const char* a, const char* b)
{
    while (*a && *a == *b) { a++; b++; }
    return (unsigned char)*a - (unsigned char)*b;
}

int strncmp(const char*a, const char* b, size_t n)
{
    for (size_t i = 0; i < n; i++)
    {
        if (a[i] != b[i])
            return (unsigned char)a[i] - (unsigned char)b[i];
        if (a[i] == '\0')
            return 0;
    }
    return 0;
}

char* strcpy(char* dst, const char* src)
{
    char* ret = dst;
    while ((*dst++ = *src++)) { }
    return ret;
}

char* strncpy(char* dst, const char* src, size_t n)
{
    size_t i;
    for (i = 0; i < n && src[i]; i++)
        dst[i] = src[i];
    for (; i < n; i++)
        dst[i] = '\0';  // Pad the rest with NUL's, like the real strncpy
    return dst;
}

char* strchr(const char* s, int c)
{
    while (*s)
    {
        if (*s == (char)c)
            return (char*)s;
        s++;
    }
    // real strchr treats the NUL terminator as part of the string
    // for the purposes of searching for '\0' itself
    return (c == '\0') ? (char*)s : 0;
}

char* strrchr(const char* s, int c)
{
    const char* last = 0;
    while (*s)
    {
        if (*s == (char)c)
            last = s;
        s++;
    }
    if (c == '\0')
        return (char*)s;
    return (char*)last;
}

void* memcpy (void* dst, const void* src, size_t n)
{
    unsigned char* d = dst;
    const unsigned char* s = src;
    for (size_t i = 0; i < n; i++)
        d[i] = s[i];
    return dst;
}

void* memset(void* dst, int value, size_t n)
{
    unsigned char* d = dst;
    for (size_t i = 0; i < n; i++)
        d[i] = (unsigned char)value;
    return dst;
}

void* memmove(void* dst, const void* src, size_t n)
{
    unsigned char* d = dst;
    const unsigned char* s = src;

    if (d == s || n == 0)
        return dst;

    if (d < s)
    {
        // Destination is before source, copying forward cant clobber
        // data we havent read yet.
        for (size_t i = 0; i < n; i++)
            d[i] = s[i];
    }
    else
    {
        // Destination is after source and the regions may overlap,
        // Copy from the tain backward instead.
        for (size_t i = n; i > 0; i--)
            d[i - 1] = s[i - 1];
    }
    return dst;
}