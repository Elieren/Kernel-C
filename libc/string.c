#include <stddef.h>
#include "string.h"
#include <stdint.h>

/* =================== MEM =================== */
void *memcpy(void *dst, const void *src, size_t n)
{
    unsigned char *d = (unsigned char *)dst;
    const unsigned char *s = (const unsigned char *)src;
    for (size_t i = 0; i < n; ++i)
        d[i] = s[i];
    return dst;
}

void *memset(void *s, int c, size_t n)
{
    unsigned char *p = (unsigned char *)s;
    for (size_t i = 0; i < n; ++i)
        p[i] = (unsigned char)c;
    return s;
}

int memcmp(const void *ptr1, const void *ptr2, size_t num)
{
    const unsigned char *a = (const unsigned char *)ptr1;
    const unsigned char *b = (const unsigned char *)ptr2;

    for (size_t i = 0; i < num; i++)
    {
        if (a[i] != b[i])
            return (int)a[i] - (int)b[i];
    }
    return 0;
}

void *memmove(void *dst0, const void *src0, size_t n)
{
    if (n == 0 || dst0 == src0)
        return dst0;

    unsigned char *dst = (unsigned char *)dst0;
    const unsigned char *src = (const unsigned char *)src0;

    if (dst < src) /* копируем вперед */
    {
        size_t word = sizeof(uintptr_t);
        while (n && ((uintptr_t)dst & (word - 1)))
        {
            *dst++ = *src++;
            --n;
        }

        uintptr_t *dw = (uintptr_t *)dst;
        const uintptr_t *sw = (const uintptr_t *)src;
        while (n >= word)
        {
            *dw++ = *sw++;
            n -= word;
        }

        dst = (unsigned char *)dw;
        src = (const unsigned char *)sw;
        while (n--)
            *dst++ = *src++;
    }
    else /* dst > src — копируем назад */
    {
        dst += n;
        src += n;

        size_t word = sizeof(uintptr_t);
        while (n && ((uintptr_t)dst & (word - 1)))
        {
            *--dst = *--src;
            --n;
        }

        uintptr_t *dw = (uintptr_t *)dst;
        const uintptr_t *sw = (const uintptr_t *)src;
        while (n >= word)
        {
            *--dw = *--sw;
            n -= word;
        }

        dst = (unsigned char *)dw;
        src = (const unsigned char *)sw;
        while (n--)
            *--dst = *--src;
    }

    return dst0;
}

/* =================== STR =================== */
size_t strlen(const char *s)
{
    size_t len = 0;
    while (*s++)
        len++;
    return len;
}

char *strcpy(char *dst, const char *src)
{
    char *d = dst;
    while ((*d++ = *src++))
        ;
    return dst;
}

char *strncpy(char *dst, const char *src, size_t n)
{
    size_t i = 0;
    for (; i < n && src[i]; i++)
        dst[i] = src[i];
    for (; i < n; i++)
        dst[i] = '\0';
    return dst;
}

char *strcat(char *dst, const char *src)
{
    char *d = dst;
    while (*d)
        d++;
    while ((*d++ = *src++))
        ;
    return dst;
}

int strcmp(const char *a, const char *b)
{
    while (*a && (*a == *b))
    {
        a++;
        b++;
    }
    return *(unsigned char *)a - *(unsigned char *)b;
}

int strncmp(const char *a, const char *b, size_t n)
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

char *strchr(const char *s, int c)
{
    while (*s)
    {
        if (*s == (char)c)
            return (char *)s;
        s++;
    }
    return NULL;
}

char *strrchr(const char *s, int c)
{
    const char *last = NULL;
    while (*s)
    {
        if (*s == (char)c)
            last = s;
        s++;
    }
    return (char *)last;
}

char *strncat(char *dest, const char *src, size_t n)
{
    char *d = dest;
    while (*d)
        ++d; /* найдём конец dest */
    size_t i = 0;
    while (i < n && src[i] != '\0')
    {
        d[i] = src[i];
        ++i;
    }
    d[i] = '\0';
    return dest;
}

char *strtok_r(char *str, const char *delim, char **saveptr)
{
    char *token;

    if (str)
        *saveptr = str;
    if (*saveptr == NULL)
        return NULL;

    // Пропускаем ведущие символы-разделители
    char *start = *saveptr;
    while (*start && strchr(delim, *start))
        start++;
    if (*start == '\0')
    {
        *saveptr = NULL;
        return NULL;
    }

    // Найти конец токена
    token = start;
    char *p = start;
    while (*p && !strchr(delim, *p))
        p++;

    if (*p)
    {
        *p = '\0';
        *saveptr = p + 1;
    }
    else
    {
        *saveptr = NULL;
    }

    return token;
}

int nameeq(const char *a, const char *b, size_t n)
{
    for (size_t i = 0; i < n; ++i)
    {
        char ca = a[i], cb = b[i];
        if (!ca && !cb)
            return 1;
        if (ca != cb)
            return 0;
    }
    return 1;
}