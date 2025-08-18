// syscall.c
#include "syscall.h"
#include "../vga/vga.h"
#include "../time/timer.h"

extern uint32_t seconds;

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
    case 0:
        print_char((char)a1, a2, a3, (uint8_t)a4, (uint8_t)a5);
        return 0;
    case 1:
        uint_to_str(seconds, str);
        return (uint32_t)str;
    case 2:
        print_string((const char *)a1, a2, a3, (uint8_t)a4, (uint8_t)a5);
        return 0;
    default:
        return (uint32_t)-1;
    }
}