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

int strncmp(const char* a, const char* b, size_t n)
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

void* memcpy(void* dst, const void* src, size_t n)
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
