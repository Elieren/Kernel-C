#include "vga.h"
#include "../portio/portio.h"
#include "../time/timer.h"
#include "../time/clock/clock.h"
#include <stdlib.h>

enum vga_mode current_mode = TEXT_MODE;

#define VGA_GRAPHICS_BUF ((uint8_t *)0xA0000)
#define VGA_GRAPHICS_WIDTH 320
#define VGA_GRAPHICS_HEIGHT 200

#define VGA_BUF ((uint8_t *)0xB8000)

#define VGA_WIDTH 80
#define VGA_HEIGHT 25

#define VGA_CTRL 0x3D4
#define VGA_DATA 0x3D5
#define CURSOR_HIGH 0x0E
#define CURSOR_LOW 0x0F

#define TAB_SIZE 8

extern uint32_t seconds;

char time_str[9];
char time_up[9];

uint8_t x;
uint8_t y;

uint8_t make_color(const uint8_t fore, const uint8_t back)
{
    return (back << 4) | (fore & 0x0F);
}

void clean_screen(void) {
    if (current_mode == TEXT_MODE) {
        uint8_t *vid = VGA_BUF;
        for (unsigned int i = 0; i < 80 * 25 * 2; i += 2) {
            vid[i] = ' ';
            vid[i + 1] = 0x07;
        }
        x = 0;
        y = 0;
    } else {
        clear_screen_graphics(0); // Черный цвет
    }
}

// простая функция прокрутки экрана
void scroll_screen()
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

// ============================ char ============================

void print_string_position(const char *str,
                           const unsigned int x,
                           const unsigned int y,
                           const uint8_t fore,
                           const uint8_t back)
{
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
        print_char_position(time_up[i], 70 + i, 23, YELLOW, BLACK);
    }
}

void print_time(void)
{
    format_clock(time_str, system_clock);

    for (uint32_t i = 0; time_str[i]; ++i)
    {
        print_char_position(time_str[i], 70 + i, 24, YELLOW, BLACK);
    }
}

void set_vga_mode(enum vga_mode mode) {
    asm volatile(
        "movb $0x00, %%ah\n\t"    // Устанавливаем AH = 0
        "movb %[mode], %%al\n\t"  // Передаем режим в AL
        "int $0x10"               // Вызов BIOS прерывания
        :
        : [mode] "r"((unsigned char)mode)  // Используем именованный операнд
        : "ah", "al"                     // Загрязняемые регистры
    );
}

void draw_pixel(uint16_t x, uint16_t y, uint8_t color) {
    if (x >= VGA_GRAPHICS_WIDTH || y >= VGA_GRAPHICS_HEIGHT)
        return;

    uint16_t offset = y * VGA_GRAPHICS_WIDTH + x;
    ((uint8_t *)VGA_GRAPHICS_BUF)[offset] = color;
}

void clear_screen_graphics(uint8_t color) {
    for (uint16_t i = 0; i < VGA_GRAPHICS_WIDTH * VGA_GRAPHICS_HEIGHT; i++) {
        ((uint8_t *)VGA_GRAPHICS_BUF)[i] = color;
    }
}

void draw_line(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint8_t color) {
    int dx = abs(x2 - x1);
    int dy = abs(y2 - y1);
    int sx = x1 < x2 ? 1 : -1;
    int sy = y1 < y2 ? 1 : -1;
    int err = dx - dy;

    while (1) {
        draw_pixel(x1, y1, color);
        if (x1 == x2 && y1 == y2) break;
        int e2 = 2 * err;
        if (e2 > -dy) { err -= dy; x1 += sx; }
        if (e2 < dx) { err += dx; y1 += sy; }
    }
}

// Функция отрисовки прямоугольника
void draw_rectangle(uint16_t x, uint16_t y, uint16_t width, uint16_t height, uint8_t color) {
    draw_line(x, y, x + width, y, color);        // Верхняя линия
    draw_line(x + width, y, x + width, y + height, color); // Правая линия
    draw_line(x + width, y + height, x, y + height, color); // Нижняя линия
    draw_line(x, y + height, x, y, color);       // Левая линия
}

// Функция отрисовки заполненного прямоугольника
void draw_filled_rectangle(uint16_t x, uint16_t y, uint16_t width, uint16_t height, uint8_t color) {
    for (uint16_t i = 0; i < height; i++) {
        for (uint16_t j = 0; j < width; j++) {
            draw_pixel(x + j, y + i, color);
        }
    }
}

// Функция отрисовки окружности (алгоритм средней точки)
void draw_circle(uint16_t x0, uint16_t y0, uint16_t radius, uint8_t color) {
    int x = radius;
    int y = 0;
    int err = 0;

    while (x >= y) {
        draw_pixel(x0 + x, y0 + y, color);
        draw_pixel(x0 + y, y0 + x, color);
        draw_pixel(x0 - y, y0 + x, color);
        draw_pixel(x0 - x, y0 + y, color);
        draw_pixel(x0 - x, y0 - y, color);
        draw_pixel(x0 - y, y0 - x, color);
        draw_pixel(x0 + y, y0 - x, color);
        draw_pixel(x0 + x, y0 - y, color);
        
        y++;
        err = err + 1 + 2*y;
        
        if (2*(err - x) + 1 > 0) {
            x--;
            err = err + 1 - 2*x;
        }
    }
}

// Функция отрисовки заполненной окружности
void draw_filled_circle(uint16_t x0, uint16_t y0, uint16_t radius, uint8_t color) {
    int x = radius;
    int y = 0;
    int err = 0;

    while (x >= y) {
        // Рисуем горизонтальные линии для заполнения
        for (int i = -x; i <= x; i++) {
            draw_pixel(x0 + i, y0 + y, color);
            draw_pixel(x0 + i, y0 - y, color);
        }
        
        for (int i = -y; i <= y; i++) {
            draw_pixel(x0 + x, y0 + i, color);
            draw_pixel(x0 - x, y0 + i, color);
        }

        y++;
        err = err + 1 + 2*y;
        
        if (2*(err - x) + 1 > 0) {
            x--;
            err = err + 1 - 2*x;
        }
    }
}
