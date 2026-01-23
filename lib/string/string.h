// string.h
#ifndef KERNEL_STRING_H
#define KERNEL_STRING_H

#include <stddef.h>

void *memcpy(void *dst, const void *src, size_t n);
void *memset(void *s, int c, size_t n);
int memcmp(const void *ptr1, const void *ptr2, size_t num);
void *memmove(void *dst0, const void *src0, size_t n);

size_t strlen(const char *s);
char *strcpy(char *dst, const char *src);
char *strncpy(char *dst, const char *src, size_t n);
char *strcat(char *dst, const char *src);
int strcmp(const char *a, const char *b);
int strncmp(const char *a, const char *b, size_t n);
char *strchr(const char *s, int c);
char *strrchr(const char *s, int c);
char *strncat(char *dest, const char *src, size_t n);
char *strtok_r(char *str, const char *delim, char **saveptr);
int nameeq(const char *a, const char *b, size_t n);

#endif // KERNEL_STRING_H