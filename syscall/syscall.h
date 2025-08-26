#ifndef SYSCALL_H
#define SYSCALL_H

#include <stdint.h>
#include <stddef.h>
#include "../malloc/malloc.h"
#include "../multitask/multitask.h"

#define SYSCALL_PRINT_CHAR 0
#define SYSCALL_PRINT_STRING 1
#define SYSCALL_GET_TIME 2

// Syscall номера для malloc
#define SYSCALL_MALLOC 10
#define SYSCALL_REALLOC 11
#define SYSCALL_FREE 12
#define SYSCALL_KMALLOC_STATS 13

#define SYSCALL_GETCHAR 30 /* получить символ из клавиатурного буфера; -1 если пусто */
#define SYSCALL_SETPOSCURSOR 31

#define SYSCALL_POWER_OFF 100 // выключение системы
#define SYSCALL_REBOOT 101    // перезагрузка системы

// Syscall номера для мультизадачности
#define SYSCALL_TASK_CREATE 200
#define SYSCALL_TASK_LIST 201
#define SYSCALL_TASK_STOP 202
#define SYSCALL_REAP_ZOMBIES 203
#define SYSCALL_TASK_EXIT 204

// Обёртки для удобства
static inline void *sys_malloc(size_t size)
{
    register uint64_t rax asm("rax") = SYSCALL_MALLOC;
    register uint64_t rdi asm("rdi") = (uint64_t)size;
    register uint64_t rsi asm("rsi") = 0;
    register uint64_t rdx asm("rdx") = 0;
    register uint64_t r10 asm("r10") = 0;
    register uint64_t r8 asm("r8") = 0;
    register uint64_t r9 asm("r9") = 0;

    asm volatile("syscall"
                 : "+r"(rax)
                 : "r"(rdi), "r"(rsi), "r"(rdx), "r"(r10), "r"(r8), "r"(r9)
                 : "rcx", "r11", "memory");
    return (void *)rax;
}

static inline void *sys_realloc(void *ptr, size_t new_size)
{
    register uint64_t rax asm("rax") = SYSCALL_REALLOC;
    register uint64_t rdi asm("rdi") = (uint64_t)ptr;
    register uint64_t rsi asm("rsi") = (uint64_t)new_size;
    register uint64_t rdx asm("rdx") = 0;
    register uint64_t r10 asm("r10") = 0;
    register uint64_t r8 asm("r8") = 0;
    register uint64_t r9 asm("r9") = 0;

    asm volatile("syscall"
                 : "+r"(rax)
                 : "r"(rdi), "r"(rsi), "r"(rdx), "r"(r10), "r"(r8), "r"(r9)
                 : "rcx", "r11", "memory");
    return (void *)rax;
}

static inline void sys_free(void *ptr)
{
    register uint64_t rax asm("rax") = SYSCALL_FREE;
    register uint64_t rdi asm("rdi") = (uint64_t)ptr;
    register uint64_t rsi asm("rsi") = 0;
    register uint64_t rdx asm("rdx") = 0;
    register uint64_t r10 asm("r10") = 0;
    register uint64_t r8 asm("r8") = 0;
    register uint64_t r9 asm("r9") = 0;

    asm volatile("syscall"
                 : "+r"(rax)
                 : "r"(rdi), "r"(rsi), "r"(rdx), "r"(r10), "r"(r8), "r"(r9)
                 : "rcx", "r11", "memory");
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

static inline const char sys_getchar(void)
{
    asm volatile(
        "int $0x80"
        :
        : "a"(SYSCALL_GETCHAR)
        : "memory");
}

static inline void sys_setposcursor(uint32_t x, uint32_t y)
{
    asm volatile(
        "int $0x80"
        :
        : "a"(SYSCALL_SETPOSCURSOR), "b"(x), "c"(y)
        : "memory");
}

static inline const char *sys_get_seconds_str(void)
{
    register uint64_t rax asm("rax") = SYSCALL_GET_TIME;
    register uint64_t rdi asm("rdi") = 0;
    register uint64_t rsi asm("rsi") = 0;
    register uint64_t rdx asm("rdx") = 0;
    register uint64_t r10 asm("r10") = 0;
    register uint64_t r8 asm("r8") = 0;
    register uint64_t r9 asm("r9") = 0;

    asm volatile("syscall"
                 : "+r"(rax)
                 : "r"(rdi), "r"(rsi), "r"(rdx), "r"(r10), "r"(r8), "r"(r9)
                 : "rcx", "r11", "memory");
    return (const char *)rax;
}

static inline uintptr_t sys_print_char(char ch, uint32_t x, uint32_t y, uint8_t fg, uint8_t bg)
{
    register uint64_t rax asm("rax") = SYSCALL_PRINT_CHAR;
    register uint64_t rdi asm("rdi") = (uint64_t)ch;
    register uint64_t rsi asm("rsi") = (uint64_t)x;
    register uint64_t rdx asm("rdx") = (uint64_t)y;
    register uint64_t r10 asm("r10") = (uint64_t)fg;
    register uint64_t r8 asm("r8") = (uint64_t)bg;
    register uint64_t r9 asm("r9") = 0;

    asm volatile("syscall"
                 : "+r"(rax)
                 : "r"(rdi), "r"(rsi), "r"(rdx), "r"(r10), "r"(r8), "r"(r9)
                 : "rcx", "r11", "memory");

    return rax;
}

static inline uintptr_t sys_print_str(const char *s, uint32_t x, uint32_t y, uint8_t fg, uint8_t bg)
{
    register uint64_t rax asm("rax") = SYSCALL_PRINT_STRING;
    register uint64_t rdi asm("rdi") = (uint64_t)s;
    register uint64_t rsi asm("rsi") = (uint64_t)x;
    register uint64_t rdx asm("rdx") = (uint64_t)y;
    register uint64_t r10 asm("r10") = (uint64_t)fg;
    register uint64_t r8 asm("r8") = (uint64_t)bg;
    register uint64_t r9 asm("r9") = 0;

    asm volatile("syscall"
                 : "+r"(rax)
                 : "r"(rdi), "r"(rsi), "r"(rdx), "r"(r10), "r"(r8), "r"(r9)
                 : "rcx", "r11", "memory");
    return rax;
}

static inline void sys_power_off(void)
{
    asm volatile(
        "int $0x80"
        :
        : "a"(SYSCALL_POWER_OFF)
        : "memory");
}

static inline void sys_reboot(void)
{
    asm volatile(
        "int $0x80"
        :
        : "a"(SYSCALL_REBOOT)
        : "memory");
}

static inline void sys_task_create(void (*entry)(void), size_t stack_size)
{
    asm volatile(
        "int $0x80"
        :
        : "a"(SYSCALL_TASK_CREATE),
          "b"(entry),
          "c"(stack_size)
        : "memory");
}

static inline int sys_task_list(task_info_t *buf, size_t max)
{
    int ret;
    asm volatile(
        "int $0x80"
        : "=a"(ret)
        : "a"(SYSCALL_TASK_LIST),
          "b"(buf),
          "c"(max)
        : "memory");
    return ret;
}

static inline int sys_task_stop(int pid)
{
    int ret;
    asm volatile(
        "int $0x80"
        : "=a"(ret)
        : "a"(SYSCALL_TASK_STOP),
          "b"(pid)
        : "memory");
    return ret;
}

static inline void sys_reap_zombies(void)
{
    asm volatile(
        "int $0x80"
        :
        : "a"(SYSCALL_REAP_ZOMBIES)
        : "memory");
}

static inline int sys_task_exit(int code)
{
    int ret;
    asm volatile(
        "int $0x80"
        : "=a"(ret)
        : "a"(SYSCALL_TASK_EXIT),
          "b"(code)
        : "memory");
    return ret;
}

#endif // SYSCALL_H
