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
// Обертки для пользовательского кода
static inline void syscall_print_char(char c, uint32_t x, uint32_t y, uint8_t fg, uint8_t bg)
{
    __asm__ volatile(
        "movq %0, %%rax\n"
        "movq %1, %%rdi\n"
        "movq %2, %%rsi\n"
        "movq %3, %%rdx\n"
        "movq %4, %%r10\n"
        "movq %5, %%r8\n"
        "syscall\n"
        :
        : "i"((uint64_t)SYSCALL_PRINT_CHAR), "r"((uint64_t)c), "r"((uint64_t)x), "r"((uint64_t)y), "r"((uint64_t)fg), "r"((uint64_t)bg)
        : "rax", "rdi", "rsi", "rdx", "r10", "r8", "memory");
}

static inline void syscall_print_string(const char *str, uint32_t x, uint32_t y, uint8_t fg, uint8_t bg)
{
    __asm__ volatile(
        "movq %0, %%rax\n"
        "movq %1, %%rdi\n"
        "movq %2, %%rsi\n"
        "movq %3, %%rdx\n"
        "movq %4, %%r10\n"
        "movq %5, %%r8\n"
        "syscall\n"
        :
        : "i"((uint64_t)SYSCALL_PRINT_STRING), "r"((uint64_t)str), "r"((uint64_t)x), "r"((uint64_t)y), "r"((uint64_t)fg), "r"((uint64_t)bg)
        : "rax", "rdi", "rsi", "rdx", "r10", "r8", "memory");
}

static inline char *syscall_get_time(char *buf)
{
    char *result;
    __asm__ volatile(
        "movq %1, %%rax\n"
        "movq %2, %%rdi\n"
        "syscall\n"
        "movq %%rax, %0\n"
        : "=r"(result)
        : "i"((uint64_t)SYSCALL_GET_TIME), "r"((uint64_t)buf)
        : "rax", "rdi", "memory");
    return result;
}

static inline void *syscall_malloc(size_t size)
{
    void *result;
    __asm__ volatile(
        "movq %1, %%rax\n"
        "movq %2, %%rdi\n"
        "syscall\n"
        "movq %%rax, %0\n"
        : "=r"(result)
        : "i"((uint64_t)SYSCALL_MALLOC), "r"((uint64_t)size)
        : "rax", "rdi", "memory");
    return result;
}

static inline void syscall_free(void *ptr)
{
    __asm__ volatile(
        "movq %0, %%rax\n"
        "movq %1, %%rdi\n"
        "syscall\n"
        :
        : "i"((uint64_t)SYSCALL_FREE), "r"((uint64_t)ptr)
        : "rax", "rdi", "memory");
}

static inline void *syscall_realloc(void *ptr, size_t size)
{
    void *result;
    __asm__ volatile(
        "movq %1, %%rax\n"
        "movq %2, %%rdi\n"
        "movq %3, %%rsi\n"
        "syscall\n"
        "movq %%rax, %0\n"
        : "=r"(result)
        : "i"((uint64_t)SYSCALL_REALLOC), "r"((uint64_t)ptr), "r"((uint64_t)size)
        : "rax", "rdi", "rsi", "memory");
    return result;
}

static inline void syscall_kmalloc_stats(void *stats)
{
    __asm__ volatile(
        "movq %0, %%rax\n"
        "movq %1, %%rdi\n"
        "syscall\n"
        :
        : "i"((uint64_t)SYSCALL_KMALLOC_STATS), "r"((uint64_t)stats)
        : "rax", "rdi", "memory");
}

static inline int syscall_getchar(void)
{
    int result;
    __asm__ volatile(
        "movq %1, %%rax\n"
        "syscall\n"
        "movq %%rax, %0\n"
        : "=r"(result)
        : "i"((uint64_t)SYSCALL_GETCHAR)
        : "rax", "memory");
    return result;
}

static inline void syscall_setposcursor(uint8_t x, uint8_t y)
{
    __asm__ volatile(
        "movq %0, %%rax\n"
        "movq %1, %%rdi\n"
        "movq %2, %%rsi\n"
        "syscall\n"
        :
        : "i"((uint64_t)SYSCALL_SETPOSCURSOR), "r"((uint64_t)x), "r"((uint64_t)y)
        : "rax", "rdi", "rsi", "memory");
}

static inline void syscall_power_off(void)
{
    __asm__ volatile(
        "movq %0, %%rax\n"
        "syscall\n"
        :
        : "i"((uint64_t)SYSCALL_POWER_OFF)
        : "rax", "memory");
}

static inline void syscall_reboot(void)
{
    __asm__ volatile(
        "movq %0, %%rax\n"
        "syscall\n"
        :
        : "i"((uint64_t)SYSCALL_REBOOT)
        : "rax", "memory");
}

static inline void syscall_task_create(void (*entry)(void), size_t stack_size)
{
    __asm__ volatile(
        "movq %0, %%rax\n"
        "movq %1, %%rdi\n"
        "movq %2, %%rsi\n"
        "syscall\n"
        :
        : "i"((uint64_t)SYSCALL_TASK_CREATE), "r"((uint64_t)entry), "r"((uint64_t)stack_size)
        : "rax", "rdi", "rsi", "memory");
}

static inline int syscall_task_list(void *buf, size_t max)
{
    int result;
    __asm__ volatile(
        "movq %1, %%rax\n"
        "movq %2, %%rdi\n"
        "movq %3, %%rsi\n"
        "syscall\n"
        "movq %%rax, %0\n"
        : "=r"(result)
        : "i"((uint64_t)SYSCALL_TASK_LIST), "r"((uint64_t)buf), "r"((uint64_t)max)
        : "rax", "rdi", "rsi", "memory");
    return result;
}

static inline int syscall_task_stop(int pid)
{
    int result;
    __asm__ volatile(
        "movq %1, %%rax\n"
        "movq %2, %%rdi\n"
        "syscall\n"
        "movq %%rax, %0\n"
        : "=r"(result)
        : "i"((uint64_t)SYSCALL_TASK_STOP), "r"((uint64_t)pid)
        : "rax", "rdi", "memory");
    return result;
}

static inline void syscall_reap_zombies(void)
{
    __asm__ volatile(
        "movq %0, %%rax\n"
        "syscall\n"
        :
        : "i"((uint64_t)SYSCALL_REAP_ZOMBIES)
        : "rax", "memory");
}

static inline void syscall_task_exit(int exit_code)
{
    __asm__ volatile(
        "movq %0, %%rax\n"
        "movq %1, %%rdi\n"
        "syscall\n"
        :
        : "i"((uint64_t)SYSCALL_TASK_EXIT), "r"((uint64_t)exit_code)
        : "rax", "rdi", "memory");
}

#endif // SYSCALL_H