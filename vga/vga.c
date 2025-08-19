#include "vga.h"
#include "../portio/portio.h"
#include "../time/timer.h"
#include "../time/clock/clock.h"

#define VGA_BUF ((uint8_t *)0xB8000)

#define VGA_WIDTH 80
#define VGA_HEIGHT 25

#define VGA_CTRL 0x3D4
#define VGA_DATA 0x3D5
#define CURSOR_HIGH 0x0E
#define CURSOR_LOW 0x0F

extern uint32_t seconds;

char time_str[9];
char time_up[9];

void clean_screen(void)
{
    uint8_t *vid = VGA_BUF;

    for (unsigned int i = 0; i < 80 * 25 * 2; i += 2)
    {
        vid[i] = ' ';      // сам символ
        vid[i + 1] = 0x07; // атрибут цвета
    }
}

uint8_t make_color(const uint8_t fore, const uint8_t back)
{
    return (back << 4) | (fore & 0x0F);
}

void print_char(const char c,
                const unsigned int x,
                const unsigned int y,
                const uint8_t fore,
                const uint8_t back)
{
    // проверка границ экрана
    if (x >= VGA_WIDTH || y >= VGA_HEIGHT)
        return;

    uint8_t *vid = VGA_BUF;
    uint8_t color = make_color(fore, back);

    // вычисляем смещение в байтах
    unsigned int offset = (y * VGA_WIDTH + x) * 2;

    vid[offset] = (uint8_t)c; // ASCII‑код символа
    vid[offset + 1] = color;  // атрибут цвета
}

void print_string(const char *str,
                  const unsigned int x,
                  const unsigned int y,
                  const uint8_t fore,
                  const uint8_t back)
{
    uint8_t *vid = VGA_BUF;

    // вычисляем смещение в байтах
    unsigned int offset = (y * VGA_WIDTH + x) * 2;

    uint8_t color = make_color(fore, back);

    for (uint32_t i = 0; str[i]; ++i)
    {
        vid[offset] = (uint8_t)str[i];
        vid[offset + 1] = color;
        offset += 2;
    }
}

void update_hardware_cursor(uint8_t x, uint8_t y)
{
    uint16_t pos = y * VGA_WIDTH + x;
    // старший байт
    outb(VGA_CTRL, CURSOR_HIGH);
    outb(VGA_DATA, (pos >> 8) & 0xFF);
    // младший байт
    outb(VGA_CTRL, CURSOR_LOW);
    outb(VGA_DATA, pos & 0xFF);
}

void format_up(unsigned total_seconds, char *buffer)
{
    unsigned hours = total_seconds / 3600;
    unsigned minutes = (total_seconds % 3600) / 60;
    unsigned seconds = total_seconds % 60;

    /* Число часов может быть больше 99, если нужно — увеличить разрядность. */
    buffer[0] = '0' + (hours / 10) % 10; /* старший разряд часов */
    buffer[1] = '0' + (hours % 10);      /* младший разряд часов */
    buffer[2] = ':';
    buffer[3] = '0' + (minutes / 10);
    buffer[4] = '0' + (minutes % 10);
    buffer[5] = ':';
    buffer[6] = '0' + (seconds / 10);
    buffer[7] = '0' + (seconds % 10);
    buffer[8] = '\0';
}

void print_systemup(void)
{
    format_up(seconds, time_up);

    for (uint32_t i = 0; time_up[i]; ++i)
    {
        print_char(time_up[i], 70 + i, 23, YELLOW, BLACK);
    }
}

void print_time(void)
{
    format_clock(time_str, system_clock);

    for (uint32_t i = 0; time_str[i]; ++i)
    {
        print_char(time_str[i], 70 + i, 24, YELLOW, BLACK);
    }
}