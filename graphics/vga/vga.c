#include "vga.h"
#include "../../portio/portio.h"

#define VGA_BUF ((uint8_t *)0xB8000)
#define VGA_WIDTH 80
#define VGA_HEIGHT 25
#define VGA_CTRL 0x3D4
#define VGA_DATA 0x3D5
#define CURSOR_HIGH 0x0E
#define CURSOR_LOW 0x0F
#define TAB_SIZE 8

uint8_t x;
uint8_t y;

static inline uint8_t make_color(const uint8_t fore, const uint8_t back)
{
    return (back << 4) | (fore & 0x0F);
}

void clean_screen(void)
{
    uint8_t *vid = VGA_BUF;

    for (unsigned int i = 0; i < 80 * 25 * 2; i += 2)
    {
        vid[i] = ' ';      // сам символ
        vid[i + 1] = 0x07; // атрибут цвета
    }

    x = 0;
    y = 0;
    update_hardware_cursor(0, 0);
}

// простая функция прокрутки экрана
void scroll_screen(void)
{
    uint16_t *vid = (uint16_t *)VGA_BUF;

    // сдвигаем всё вверх на одну строку
    for (int i = 0; i < (VGA_HEIGHT - 1) * VGA_WIDTH; i++)
    {
        vid[i] = vid[i + VGA_WIDTH];
    }

    // очищаем последнюю строку
    for (int i = (VGA_HEIGHT - 1) * VGA_WIDTH; i < VGA_HEIGHT * VGA_WIDTH; i++)
    {
        vid[i] = (uint16_t)make_color(BLACK, BLACK) << 8 | ' ';
    }

    if (y > 0)
        y--;
}

// ============================ char ============================

void print_char_position(const char c,
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

void print_char(const char c,
                const uint8_t fore,
                const uint8_t back)
{
    uint8_t *vid = VGA_BUF;
    uint8_t color = make_color(fore, back);

    if (c == '\n')
    {
        x = 0;
        y++;
        if (y >= VGA_HEIGHT)
        {
            scroll_screen();
            y = VGA_HEIGHT - 1;
        }
        update_hardware_cursor(x, y);
        return;
    }

    // вычисляем смещение в байтах
    unsigned int offset = (y * VGA_WIDTH + x) * 2;

    vid[offset] = (uint8_t)c; // ASCII-код символа
    vid[offset + 1] = color;  // атрибут цвета

    // двигаем курсор
    x++;
    if (x >= VGA_WIDTH)
    {
        x = 0;
        y++;
        if (y >= VGA_HEIGHT)
        {
            scroll_screen();
            y = VGA_HEIGHT - 1;
        }
    }
    update_hardware_cursor(x, y);
}

// ============================ string ============================

void print_string_position(const char *str,
                           const unsigned int x,
                           const unsigned int y,
                           const uint8_t fore,
                           const uint8_t back)
{
    // проверка границ экрана
    if (x >= VGA_WIDTH || y >= VGA_HEIGHT)
        return;

    uint8_t *vid = VGA_BUF;
    unsigned int offset = (y * VGA_WIDTH + x) * 2;
    uint8_t color = make_color(fore, back);

    unsigned int col = x; // текущая колонка

    for (uint32_t i = 0; str[i]; ++i)
    {
        char c = str[i];

        if (c == '\t')
        {
            // считаем сколько пробелов до следующего кратного TAB_SIZE
            unsigned int spaces = TAB_SIZE - (col % TAB_SIZE);
            for (unsigned int s = 0; s < spaces; s++)
            {
                vid[offset] = ' ';
                vid[offset + 1] = color;
                offset += 2;
                col++;
            }
        }
        else
        {
            vid[offset] = (uint8_t)c;
            vid[offset + 1] = color;
            offset += 2;
            col++;
        }

        // если дошли до конца строки VGA
        if (col >= VGA_WIDTH)
            break; // (или можно сделать перенос)
    }
}

void print_string(const char *str,
                  const uint8_t fore,
                  const uint8_t back)
{
    for (const char *p = str; *p; p++)
    {
        if (*p == '\n')
        {
            x = 0;
            y++;
            if (y >= VGA_HEIGHT)
            {
                scroll_screen();
                y = VGA_HEIGHT - 1;
            }
        }
        else
        {
            print_char(*p, fore, back);
        }
    }
    update_hardware_cursor(x, y);
}

void backspace(void)
{
    if (x == 0)
    {
        if (y > 0)
        {
            y--;
            x = VGA_WIDTH - 1;
        }
    }
    else
    {
        x--;
    }

    // Стереть символ на месте
    unsigned int offset = (y * VGA_WIDTH + x) * 2;
    uint8_t *vid = VGA_BUF;
    vid[offset] = ' ';
    vid[offset + 1] = make_color(BLACK, BLACK);

    update_hardware_cursor(x, y);
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