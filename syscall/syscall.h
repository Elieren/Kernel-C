#ifndef SYSCALL_H
#define SYSCALL_H

#include <stdint.h>
#include <stddef.h>           // для size_t
#include "../malloc/malloc.h" // для kmalloc_stats_t
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
    return ret; // возвращает число реально записанных задач
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
