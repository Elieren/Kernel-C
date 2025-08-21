// syscall.c
#include "syscall.h"
#include "../vga/vga.h"
#include "../time/timer.h"
#include "../malloc/malloc.h"
#include "../power/poweroff.h"
#include "../power/reboot.h"
#include "../keyboard/keyboard.h"
#include "../multitask/multitask.h"

extern uint32_t seconds;
extern volatile task_t *syscall_caller;

char str[11];
char tmp[11];

char *uint_to_str(uint32_t value, char *buf)
{
    int len = 0;

    // Спец-случай: ноль
    if (value == 0)
    {
        buf[0] = '0';
        buf[1] = '\0';
        return buf;
    }

    // Собираем цифры в обратном порядке
    while (value > 0)
    {
        tmp[len++] = '0' + (value % 10);
        value /= 10;
    }

    // Переворачиваем и добавляем '\0'
    for (int i = 0; i < len; i++)
    {
        buf[i] = tmp[len - 1 - i];
    }
    buf[len] = '\0';

    return buf;
}

uint32_t syscall_handler(
    uint32_t num, // EAX
    uint32_t a1,  // EBX
    uint32_t a2,  // ECX
    uint32_t a3,  // EDX
    uint32_t a4,  // ESI
    uint32_t a5,  // EDI
    uint32_t a6   // EBP
)
{
    switch (num)
    {
    case SYSCALL_PRINT_CHAR:
        print_char((char)a1, a2, a3, (uint8_t)a4, (uint8_t)a5);
        return 0;

    case SYSCALL_PRINT_STRING:
        print_string((const char *)a1, a2, a3, (uint8_t)a4, (uint8_t)a5);
        return 0;

    case SYSCALL_GET_TIME:
        uint_to_str(seconds, str);
        return (uint32_t)str;

    case SYSCALL_MALLOC:
        return (uint32_t)malloc((size_t)a1); // a1 = размер

    case SYSCALL_FREE:
        free((void *)a1); // a1 = указатель
        return 0;

    case SYSCALL_REALLOC:
        return (uint32_t)realloc((void *)a1, (size_t)a2); // a1 = ptr, a2 = new_size

    case SYSCALL_KMALLOC_STATS:
        if (a1)
        {
            get_kmalloc_stats((kmalloc_stats_t *)a1); // a1 = указатель на структуру
        }
        return 0;

    case SYSCALL_GETCHAR:
    {
        char c = kbd_getchar(); /* возвращает -1 если пусто */
        if (c == -1)
            return '\0'; /* пустой символ */
        return c;        /* возвращаем сразу char */
    }

    case SYSCALL_SETPOSCURSOR:
    {
        update_hardware_cursor((uint8_t)a1, (uint8_t)a2);
        return 0;
    }

    case SYSCALL_POWER_OFF:
        power_off();
        return 0; // на самом деле ядро выключится и сюда не вернётся

    case SYSCALL_REBOOT:
        reboot_system();
        return 0; // ядро перезагрузится

    case SYSCALL_TASK_CREATE:
        task_create((void (*)(void))a1, (size_t)a2);
        return 0;

    case SYSCALL_TASK_LIST:
        return task_list((task_info_t *)a1, a2);

    case SYSCALL_TASK_STOP:
        return task_stop((int)a1);

    case SYSCALL_REAP_ZOMBIES:
        reap_zombies();
        return 0;

    case SYSCALL_TASK_EXIT:
    {
        task_exit((int)a1);
        return 0;
    }

    default:
        return (uint32_t)-1;
    }
}