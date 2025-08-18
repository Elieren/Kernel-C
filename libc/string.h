// string.h
#ifndef KERNEL_STRING_H
#define KERNEL_STRING_H

#include <stddef.h>

void *memcpy(void *dst, const void *src, size_t n);
void *memset(void *s, int c, size_t n);
char *strcpy(char *dst, const char *src);

#endif // KERNEL_STRING_H