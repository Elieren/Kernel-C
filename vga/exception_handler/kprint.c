#include "../graphics.h"
#include "kprint.h"
#include "../../libc/string.h"
#include <stdarg.h>

// Вспомогательная функция: преобразует int в строку
static void itoa(int value, char *str, int base)
{
    const char digits[] = "0123456789ABCDEF";
    char buffer[32];
    int i = 0, j = 0;

    // Проверка на валидность буфера
    if (!str)
        return;

    // Обработка отрицательных чисел
    if (value < 0 && base == 10)
    {
        if (j < 31)
            str[j++] = '-';
        value = -value;
    }

    // Преобразование в буфер
    do
    {
        buffer[i++] = digits[value % base];
        value /= base;
    } while (value && i < 32);

    // Копирование в выходную строку с проверкой границ
    while (i-- && j < 31)
    {
        str[j++] = buffer[i];
    }
    str[j] = '\0';
}

// Так же вспомогательная функция для форматирования
static int simple_vsnprintf(char *buf, size_t size, const char *fmt, va_list args)
{
    char *p = buf;
    const char *s;
    int d;
    char temp[32];

    if (size == 0)
        return 0;

    while (*fmt && p - buf < (int)(size - 1))
    {
        if (*fmt != '%')
        {
            *p++ = *fmt++;
            continue;
        }
        fmt++;

        switch (*fmt)
        {
        case 's':
            s = va_arg(args, char *);
            if (!s)
            {
                s = "(null)";
            }
            while (*s && p - buf < (int)(size - 1))
            {
                *p++ = *s++;
            }
            break;
        case 'd':
            d = va_arg(args, int);
            itoa(d, temp, 10); // Используем свою реализацию
            for (int i = 0; temp[i] && p - buf < (int)(size - 1); i++)
            {
                *p++ = temp[i];
            }
            break;
        case 'u':
            d = va_arg(args, unsigned int);
            itoa(d, temp, 10);
            for (int i = 0; temp[i] && p - buf < (int)(size - 1); i++)
            {
                *p++ = temp[i];
            }
            break;
        case 'x':
            d = va_arg(args, int);
            itoa(d, temp, 16);
            for (int i = 0; temp[i] && p - buf < (int)(size - 1); i++)
            {
                *p++ = temp[i];
            }
            break;
        case 'c':
            *p++ = (char)va_arg(args, int);
            break;
        case '%':
            *p++ = '%';
            break;
        default:
            *p++ = '%';
            *p++ = *fmt;
            break;
        }
        fmt++;
    }
    *p = '\0';
    return p - buf;
}

int kprint(const uint8_t type, const char *format, ...)
{
    // Проверка входных данных
    if (format == NULL)
    {
        gfx_put_string("PRINT ERROR: Format string is NULL\n", 0x00FF0000);
        return -1;
    }

    // Определение цвета в зависимости от типа
    uint32_t fg = 0x00FFFFFF;
    switch (type)
    {
    case KPRINT_LOG:
        fg = 0x00FFFF00;
        break;
    case KPRINT_ERROR:
        fg = 0x00FF0000;
        break;
    case KPRINT_SUCCESS:
        fg = 0x0000FF00;
        break;
    case KPRINT_NORMAL:
        fg = 0x00FFFFFF;
        break;
    default:
        gfx_put_string("PRINT ERROR: Invalid 'kprint' argument (type)\n", 0x00FF0000);
        return -1;
    }

// Буфер для сформированного сообщения
#define BUFFER_SIZE 512
    char buffer[BUFFER_SIZE];

    // Инициализация списка аргументов
    va_list args;
    va_start(args, format);

    // Форматирование строки (аналог vsprintf)
    int result = simple_vsnprintf(buffer, BUFFER_SIZE, format, args);

    va_end(args);

    // Проверка на ошибку форматирования
    if (result < 0)
    {
        gfx_put_string("PRINT ERROR: String formatting failed\n", 0x00FF0000);
        return -1;
    }

    // Вывод отформатированного сообщения
    gfx_put_string(buffer, fg);

    return 0;
}