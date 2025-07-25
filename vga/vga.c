#include "vga.h"

#define VGA_WIDTH 80
#define VGA_HEIGHT 25

void clean_screen(void)
{
    uint8_t *vid = (uint8_t *)0xB8000;

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

    uint8_t *vid = (uint8_t *)0xB8000;
    uint8_t color = make_color(fore, back);

    // вычисляем смещение в байтах
    unsigned int offset = (y * VGA_WIDTH + x) * 2;

    vid[offset] = (uint8_t)c; // ASCII‑код символа
    vid[offset + 1] = color;  // атрибут цвета
}