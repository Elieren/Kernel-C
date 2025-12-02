#include "../framebuffer/graphics.h"
#include "kprint.h"
#include "../../libc/string.h"
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>

static void utoa_unsigned(unsigned long long value, char *out, int base)
{
    const char digits[] = "0123456789ABCDEF";
    char tmp[32];
    int pos = 0;
    if (!out)
        return;
    if (value == 0)
    {
        out[0] = '0';
        out[1] = '\0';
        return;
    }
    while (value && pos < (int)(sizeof(tmp) - 1))
    {
        tmp[pos++] = digits[value % base];
        value /= base;
    }
    int i = 0;
    while (pos > 0)
    {
        out[i++] = tmp[--pos];
    }
    out[i] = '\0';
}

static void itoa_signed(long long value, char *out)
{
    if (!out)
        return;
    if (value < 0)
    {
        *out++ = '-';
        unsigned long long u = (unsigned long long)(-(value + 1)) + 1ULL;
        utoa_unsigned(u, out, 10);
    }
    else
    {
        utoa_unsigned((unsigned long long)value, out, 10);
    }
}

static void append_padded(char **pp, char *end, const char *s, int width, char pad)
{
    if (!pp || !end || !s)
        return;
    int len = 0;
    const char *t = s;
    while (*t++)
        len++;
    int padcnt = (width > len) ? (width - len) : 0;
    while (padcnt-- > 0 && *pp < end - 1)
    {
        *(*pp)++ = pad;
    }
    for (int i = 0; i < len && *pp < end - 1; ++i)
    {
        *(*pp)++ = s[i];
    }
}

static int simple_vsnprintf(char *buf, size_t size, const char *fmt, va_list args)
{
    if (!buf || size == 0 || !fmt)
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
        int is_long = 0;
        int is_longlong = 0;

        if (*fmt == 'l')
        {
            is_long = 1;
            fmt++;
            if (*fmt == 'l') // ll
            {
                is_longlong = 1;
                fmt++;
            }
        }
        else if (*fmt == 'h')
        {
            fmt++;
            if (*fmt == 'h') // hh
                fmt++;
        }

        char temp[64];
        switch (*fmt)
        {
        case 's':
        {
            const char *s = va_arg(args, const char *);
            if (!s)
                s = "(null)";
            append_padded(&p, end, s, width, padchar);
            break;
        }
        case 'd':
        case 'i':
        {
            long long v;
            if (is_longlong)
                v = va_arg(args, long long);
            else if (is_long)
                v = va_arg(args, long);
            else
                v = va_arg(args, int);
            itoa_signed(v, temp);
            append_padded(&p, end, temp, width, padchar);
            break;
        }
        case 'u':
        {
            unsigned long long v;
            if (is_longlong)
                v = va_arg(args, unsigned long long);
            else if (is_long)
                v = va_arg(args, unsigned long);
            else
                v = va_arg(args, unsigned int);
            utoa_unsigned(v, temp, 10);
            append_padded(&p, end, temp, width, padchar);
            break;
        }
        case 'x':
        case 'X':
        {
            unsigned long long v;
            if (is_longlong)
                v = va_arg(args, unsigned long long);
            else if (is_long)
                v = va_arg(args, unsigned long);
            else
                v = va_arg(args, unsigned int);
            utoa_unsigned(v, temp, 16);
            append_padded(&p, end, temp, width, padchar);
            break;
        }
        case 'p': // Указатель
        {
            void *ptr = va_arg(args, void *);
            if (p < end - 2)
            {
                *p++ = '0';
                *p++ = 'x';
            }
            utoa_unsigned((unsigned long long)(uintptr_t)ptr, temp, 16);
            append_padded(&p, end, temp, width, padchar);
            break;
        }
        case 'c':
        {
            int ch = va_arg(args, int);
            if (p < end - 1)
                *p++ = (char)ch;
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

int kprint(const uint8_t type, const char *format, ...)
{
    if (!format)
    {
        gfx_put_string("PRINT ERROR: Format string is NULL\n", 0x00FF0000);
        return -1;
    }

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

    gfx_put_string(buffer, fg);
    return 0;
}