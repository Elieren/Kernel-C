#ifndef SYSCALL_H
#define SYSCALL_H

#include <stdint.h>
#include <stddef.h>           // для size_t
#include "../malloc/malloc.h" // для kmalloc_stats_t

#define SYSCALL_PRINT_CHAR 0
#define SYSCALL_PRINT_STRING 1
#define SYSCALL_GET_TIME 2

// Syscall номера для malloc
#define SYSCALL_MALLOC 10
#define SYSCALL_REALLOC 11
#define SYSCALL_FREE 12
#define SYSCALL_KMALLOC_STATS 13

// Обёртки для удобства
static inline void *sys_malloc(size_t size)
{
    uint32_t ret;
    asm volatile(
        "int $0x80"
        : "=a"(ret)
        : "a"(SYSCALL_MALLOC),
          "b"(size)
        : "memory");
    return (void *)ret;
}

static inline void *sys_realloc(void *ptr, size_t new_size)
{
    uint32_t ret;
    asm volatile(
        "int $0x80"
        : "=a"(ret)
        : "a"(SYSCALL_REALLOC),
          "b"(ptr),
          "c"(new_size)
        : "memory");
    return (void *)ret;
}

static inline void sys_free(void *ptr)
{
    asm volatile(
        "int $0x80"
        :
        : "a"(SYSCALL_FREE),
          "b"(ptr)
        : "memory");
}

static inline void sys_get_kmalloc_stats(kmalloc_stats_t *st)
{
    asm volatile(
        "int $0x80"
        :
        : "a"(SYSCALL_KMALLOC_STATS),
          "b"(st)
        : "memory");
}

static inline const char *sys_get_seconds_str(void)
{
    uint32_t ret;
    asm volatile(
        "int $0x80"
        : "=a"(ret)
        : "a"(SYSCALL_GET_TIME) // теперь = 2
        : "memory");
    return (const char *)ret;
}

static inline void sys_print_str(const char *s, uint32_t x, uint32_t y, uint8_t fg, uint8_t bg)
{
    asm volatile(
        "int $0x80"
        :
        : "a"(SYSCALL_PRINT_STRING), "b"(s), "c"(x), "d"(y), "S"(fg), "D"(bg) // теперь = 1
        : "memory");
}

static inline void sys_print_char(char ch, uint32_t x, uint32_t y, uint8_t fg, uint8_t bg)
{
    asm volatile(
        "int $0x80"
        :
        : "a"(SYSCALL_PRINT_CHAR), "b"(ch), "c"(x), "d"(y), "S"(fg), "D"(bg)
        : "memory");
}

#endif // SYSCALL_H
