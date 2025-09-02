// syscall.c
#include "syscall.h"
#include "../vga/vga.h"
#include "../time/timer.h"
#include "../malloc/malloc.h"
#include "../power/poweroff.h"
#include "../power/reboot.h"
#include "../keyboard/keyboard.h"
#include "../multitask/multitask.h"

#include <stdint.h>
#include <stddef.h>

extern uint32_t seconds;
extern volatile task_t *syscall_caller;

static char str[11];
static char tmp[11];

static char *uint_to_str(uint32_t value, char *buf)
{
    int len = 0;

    if (value == 0)
    {
        buf[0] = '0';
        buf[1] = '\0';
        return buf;
    }

    while (value > 0)
    {
        tmp[len++] = '0' + (value % 10);
        value /= 10;
    }

    for (int i = 0; i < len; i++)
    {
        buf[i] = tmp[len - 1 - i];
    }
    buf[len] = '\0';

    return buf;
}

uintptr_t syscall_handler(
    uint64_t rax, // syscall number
    uint64_t rdi,
    uint64_t rsi,
    uint64_t rdx,
    uint64_t r10,
    uint64_t r8,
    uint64_t r9)
{
    switch ((uint32_t)rax)
    {
    case SYSCALL_PRINT_CHAR_POSITION:
        print_char_position((char)rdi, (uint32_t)rsi, (uint32_t)rdx, (uint8_t)r10, (uint8_t)r8);
        return 0;

    case SYSCALL_PRINT_STRING_POSITION:
        print_string_position((const char *)(uintptr_t)rdi, (uint32_t)rsi, (uint32_t)rdx, (uint8_t)r10, (uint8_t)r8);
        return 0;

    case SYSCALL_PRINT_CHAR:
        print_char((char)rdi, (uint8_t)rsi, (uint8_t)rdx);
        return 0;

    case SYSCALL_PRINT_STRING:
        print_string((const char *)(uintptr_t)rdi, (uint8_t)rsi, (uint8_t)rdx);
        return 0;

    case SYSCALL_BACKSPACE:
        backspace();
        return 0;

    case SYSCALL_GET_TIME:
        return (uintptr_t)uint_to_str(rdi, (char *)rsi);

    case SYSCALL_MALLOC:
        return (uintptr_t)malloc((size_t)rdi);

    case SYSCALL_FREE:
        free((void *)(uintptr_t)rdi);
        return 0;

    case SYSCALL_REALLOC:
        return (uintptr_t)realloc((void *)(uintptr_t)rdi, (size_t)rsi);

    case SYSCALL_KMALLOC_STATS:
        if (rdi)
            get_kmalloc_stats((void *)(uintptr_t)rdi);
        return 0;

    case SYSCALL_GETCHAR:
    {
        int c = kbd_getchar();
        return (uintptr_t)(c == -1 ? 0 : c);
    }

    case SYSCALL_SETPOSCURSOR:
        update_hardware_cursor((uint8_t)rdi, (uint8_t)rsi);
        return 0;

    case SYSCALL_POWER_OFF:
        power_off();
        return 0;

    case SYSCALL_REBOOT:
        reboot_system();
        return 0;

    case SYSCALL_TASK_CREATE:
        task_create((void (*)(void))(uintptr_t)rdi, (size_t)rsi);
        return 0;

    case SYSCALL_TASK_LIST:
        return (uintptr_t)task_list((void *)(uintptr_t)rdi, (size_t)rsi);

    case SYSCALL_TASK_STOP:
        return (uintptr_t)task_stop((int)rdi);

    case SYSCALL_REAP_ZOMBIES:
        reap_zombies();
        return 0;

    case SYSCALL_TASK_EXIT:
        task_exit((int)rdi);
        return 0;

    default:
        return (uintptr_t)-1;
    }
}