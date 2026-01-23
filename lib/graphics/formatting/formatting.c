#include "drivers/video/framebuffer/graphics.h"
#include "formatting.h"
#include "lib/string/string.h"
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <limits.h>

#define TEMP_BUFFER_SIZE 64
#define DIGIT_CHARS "0123456789ABCDEF"

static void utoa_unsigned(unsigned long long value, char *out, int base)
{
    char tmp[TEMP_BUFFER_SIZE];
    int pos = 0;

    if (value == 0)
    {
        out[0] = '0';
        out[1] = '\0';
        return;
    }

    while (value && pos < TEMP_BUFFER_SIZE - 1)
    {
        tmp[pos++] = DIGIT_CHARS[value % base];
        value /= base;
    }

    // Разворот строки
    for (int i = 0; i < pos; i++)
    {
        out[i] = tmp[pos - 1 - i];
    }
    out[pos] = '\0';
}

static void itoa_signed(long long value, char *out)
{
    if (value < 0)
    {
        *out++ = '-';
        if (value == LLONG_MIN)
        {
            utoa_unsigned((unsigned long long)(-(value + 1)) + 1ULL, out, 10);
        }
        else
        {
            utoa_unsigned((unsigned long long)(-value), out, 10);
        }
    }
    else
    {
        utoa_unsigned((unsigned long long)value, out, 10);
    }
}

static void append_padded(char **pp, char *end, const char *s, int width, char pad)
{
    int len = strlen(s);
    int padcnt = (width > len) ? (width - len) : 0;

    while (padcnt-- > 0 && *pp < end - 1)
    {
        *(*pp)++ = pad;
    }

    while (*s && *pp < end - 1)
    {
        *(*pp)++ = *s++;
    }
}

static int simple_vsnprintf(char *buf, size_t size, const char *fmt, va_list args)
{
    if (size == 0 || !fmt)
        return 0;

    char *p = buf;
    char *end = buf + size;

    while (*fmt && p < end - 1)
    {
        if (*fmt != '%')
        {
            *p++ = *fmt++;
            continue;
        }

        fmt++;
        char padchar = ' ';

        if (*fmt == '0')
        {
            padchar = '0';
            fmt++;
        }

        int width = 0;
        while (*fmt >= '0' && *fmt <= '9')
        {
            width = width * 10 + (*fmt - '0');
            fmt++;
        }

        // Обработка модификаторов длины
        int is_long = 0, is_longlong = 0;

        if (*fmt == 'l')
        {
            is_long = 1;
            fmt++;
            if (*fmt == 'l')
            {
                is_longlong = 1;
                fmt++;
            }
        }
        else if (*fmt == 'h')
        {
            fmt++; // Пропускаем 'h', используем int по умолчанию
            if (*fmt == 'h')
                fmt++;
        }

        char temp[TEMP_BUFFER_SIZE];

        switch (*fmt)
        {
        case 's':
        {
            const char *s = va_arg(args, const char *);
            append_padded(&p, end, s ? s : "(null)", width, padchar);
            break;
        }
        case 'd':
        case 'i':
        {
            long long v = is_longlong ? va_arg(args, long long) : is_long ? va_arg(args, long)
                                                                          : va_arg(args, int);
            itoa_signed(v, temp);
            append_padded(&p, end, temp, width, padchar);
            break;
        }
        case 'u':
        {
            unsigned long long v = is_longlong ? va_arg(args, unsigned long long) : is_long ? va_arg(args, unsigned long)
                                                                                            : va_arg(args, unsigned int);
            utoa_unsigned(v, temp, 10);
            append_padded(&p, end, temp, width, padchar);
            break;
        }
        case 'x':
        case 'X':
        {
            unsigned long long v = is_longlong ? va_arg(args, unsigned long long) : is_long ? va_arg(args, unsigned long)
                                                                                            : va_arg(args, unsigned int);
            utoa_unsigned(v, temp, 16);
            append_padded(&p, end, temp, width, padchar);
            break;
        }
        case 'p':
        {
            if (p < end - 2)
            {
                *p++ = '0';
                *p++ = 'x';
            }
            void *ptr = va_arg(args, void *);
            utoa_unsigned((unsigned long long)(uintptr_t)ptr, temp, 16);
            append_padded(&p, end, temp, width, padchar);
            break;
        }
        case 'c':
        {
            if (p < end - 1)
                *p++ = (char)va_arg(args, int);
            break;
        }
        case '%':
            if (p < end - 1)
                *p++ = '%';
            break;
        default:
            if (p < end - 1)
                *p++ = '%';
            if (*fmt && p < end - 1)
                *p++ = *fmt;
            break;
        }

        if (*fmt)
            fmt++;
    }

    *p = '\0';
    return (int)(p - buf);
}

int kformat(char *buffer, size_t size, const char *format, ...)
{
    if (!buffer || size == 0 || !format)
        return -1;

    va_list args;
    va_start(args, format);
    int result = simple_vsnprintf(buffer, size, format, args);
    va_end(args);

    return result;
}

int kprint(const uint8_t type, const char *format, ...)
{
    if (!format)
    {
        gfx_put_string("PRINT ERROR: Format string is NULL\n", 0x00FF0000);
        return -1;
    }

    uint32_t colors[] = {
        [KPRINT_NORMAL] = 0x00FFFFFF,
        [KPRINT_LOG] = 0x00FFFF00,
        [KPRINT_ERROR] = 0x00FF0000,
        [KPRINT_SUCCESS] = 0x0000FF00,
    };

    if (type >= sizeof(colors) / sizeof(colors[0]))
    {
        gfx_put_string("PRINT ERROR: Invalid 'kprint' argument (type)\n", 0x00FF0000);
        return -1;
    }

    char buffer[KPRINT_BUFFER_SIZE];
    va_list args;
    va_start(args, format);
    int r = simple_vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);

    if (r < 0)
    {
        gfx_put_string("PRINT ERROR: String formatting failed\n", 0x00FF0000);
        return -1;
    }

    gfx_put_string(buffer, colors[type]);
    return 0;
}