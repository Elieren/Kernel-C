#include <stddef.h>
#include "string.h"
#include <stdint.h>
#include "kernel/panic/panic.h"

/* =================== MEM =================== */
void *memcpy(void *dst_, const void *src_, size_t n)
{
    if (!dst_ || !src_)
    {
        panic("MEMCPY_NULL_POINTER", false, true);
    }

    unsigned char *dst = (unsigned char *)dst_;
    const unsigned char *src = (const unsigned char *)src_;
    void *ret = dst_;

    if (n == 0 || dst == src)
        return ret;

#if defined(__x86_64__) || defined(__i386__)
    /* На x86/x86_64 rep movsb очень быстрый */
    __asm__ volatile(
        "rep movsb"
        : "+D"(dst), "+S"(src), "+c"(n)
        :
        : "memory");
    return ret;
#else
    const size_t W = sizeof(unsigned long);
    uintptr_t dst_addr = (uintptr_t)dst;
    uintptr_t src_addr = (uintptr_t)src;

    /* Выравниваем dst по границе слова побайтно */
    while (n > 0 && (dst_addr & (W - 1)))
    {
        *dst++ = *src++;
        --n;
        dst_addr = (uintptr_t)dst;
        src_addr = (uintptr_t)src;
    }

    /* Если выравнивание src равно выравниванию dst, можно копировать словами */
    if ((src_addr & (W - 1)) == (dst_addr & (W - 1)))
    {
        unsigned long *dw = (unsigned long *)dst;
        const unsigned long *sw = (const unsigned long *)src;
        size_t words = n / W;

        /* Разворачивание цикла по 4 слова */
        while (words >= 4)
        {
            dw[0] = sw[0];
            dw[1] = sw[1];
            dw[2] = sw[2];
            dw[3] = sw[3];
            dw += 4;
            sw += 4;
            words -= 4;
        }
        while (words--)
        {
            *dw++ = *sw++;
        }

        /* Обновляем указатели и оставшиеся байты */
        dst = (unsigned char *)dw;
        src = (const unsigned char *)sw;
        n = n & (W - 1);
    }
    else
    {
        /* Если выравнивания не совпадают - оставляем всё побайтно.
           Это безопасно на архитектурах с требованием выравнивания. */
    }

    /* Хвостовые байты */
    while (n--)
    {
        *dst++ = *src++;
    }

    return ret;
#endif
}

void *memset(void *s, int c, size_t n)
{
    if (!s)
    {
        panic("MEMSET_NULL_POINTER", false, true);
    }

    unsigned char *p = (unsigned char *)s;
    unsigned char val = (unsigned char)c;
    void *ret = s;

    const size_t W = sizeof(unsigned long);
    uintptr_t addr = (uintptr_t)p;

    /* Выравниваем по границе слова побайтно */
    while (n > 0 && (addr & (W - 1)))
    {
        *p++ = val;
        --n;
        addr = (uintptr_t)p;
    }

    /* Заполняем целыми словами */
    if (n >= W)
    {
        unsigned long word = val;
        word |= word << 8;
        word |= word << 16;
#ifdef __LP64__
        word |= word << 32;
#endif
        unsigned long *pw = (unsigned long *)p;
        while (n >= W)
        {
            *pw++ = word;
            n -= W;
        }
        p = (unsigned char *)pw;
    }

    /* Оставшиеся байты */
    while (n--)
        *p++ = val;

    return ret;
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
    if (!dst0 || !src0)
    {
        panic("MEMMOVE_NULL_POINTER", false, true);
    }

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
    else /* dst > src - копируем назад */
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
    const char *p = s;
    while (*p)
        p++;
    return p - s;
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
    while (*a == *b && *a != '\0')
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
    if ((char)c == '\0')
        return (char *)s;
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
    if ((char)c == '\0')
        return (char *)s;
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