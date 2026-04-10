#include "vga.h"
#include <asm/io.h>

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

static void vga_update_cursor(uint32_t x, uint32_t y);

// Конвертация 0xAARRGGBB → ближайший VGA-цвет (по евклидову расстоянию)

/*
 * Приближённые RGB-значения 16 стандартных VGA-цветов.
 * Индекс соответствует vga_color.
 */
static const uint8_t k_vga_r[16] = {0,
                                    0,
                                    0,
                                    0,
                                    170,
                                    170,
                                    170,
                                    170,
                                    85,
                                    85,
                                    85,
                                    85,
                                    255,
                                    255,
                                    255,
                                    255};
static const uint8_t k_vga_g[16] = {0, 0, 170, 170, 0, 0, 85, 170,
                                    85, 85, 255, 255, 85, 85, 255, 255};
static const uint8_t k_vga_b[16] = {0, 170, 0, 170, 0, 170, 0, 170,
                                    85, 255, 85, 255, 85, 255, 85, 255};

static uint8_t rgb_to_vga(uint32_t color)
{
    uint8_t r = (uint8_t)((color >> 16) & 0xFF);
    uint8_t g = (uint8_t)((color >> 8) & 0xFF);
    uint8_t b = (uint8_t)(color & 0xFF);

    uint8_t best = 0;
    uint32_t best_dist = 0xFFFFFFFFu;

    for (int i = 0; i < 16; ++i)
    {
        int32_t dr = (int32_t)r - (int32_t)k_vga_r[i];
        int32_t dg = (int32_t)g - (int32_t)k_vga_g[i];
        int32_t db = (int32_t)b - (int32_t)k_vga_b[i];
        uint32_t dist = (uint32_t)(dr * dr + dg * dg + db * db);
        if (dist < best_dist)
        {
            best_dist = dist;
            best = (uint8_t)i;
        }
    }
    return best;
}

static inline uint8_t make_color(uint8_t fg, uint8_t bg)
{
    return (uint8_t)((bg << 4) | (fg & 0x0Fu));
}

/* Текстовая очистка: пробел + чёрный-на-чёрном, курсор в (0,0) */
static void vga_clean_screen(void)
{
    uint8_t *vid = VGA_BUF;
    uint8_t attr = make_color(BLACK, BLACK);

    for (unsigned int i = 0; i < VGA_WIDTH * VGA_HEIGHT * 2; i += 2)
    {
        vid[i] = ' ';
        vid[i + 1] = attr;
    }

    x = 0;
    y = 0;
    vga_update_cursor(0, 0);
}

/* Пиксельная заливка: конвертируем цвет в ближайший VGA-атрибут bg.
   Весь экран заполняется пробелами с этим фоном.                    */
static void vga_clear(uint32_t color)
{
    uint8_t *vid = VGA_BUF;
    uint8_t bg = rgb_to_vga(color);
    uint8_t attr = make_color(bg, bg); /* fg == bg → пробел "невидим" */

    for (unsigned int i = 0; i < VGA_WIDTH * VGA_HEIGHT * 2; i += 2)
    {
        vid[i] = ' ';
        vid[i + 1] = attr;
    }
}

// простая функция прокрутки экрана
static void vga_scroll(void)
{
    uint16_t *vid = (uint16_t *)VGA_BUF;

    // сдвигаем всё вверх на одну строку
    for (int i = 0; i < (VGA_HEIGHT - 1) * VGA_WIDTH; i++)
    {
        vid[i] = vid[i + VGA_WIDTH];
    }

    // очищаем последнюю строку
    uint8_t attr = make_color(BLACK, BLACK);
    for (int i = (VGA_HEIGHT - 1) * VGA_WIDTH; i < VGA_HEIGHT * VGA_WIDTH; i++)
    {
        vid[i] = (uint16_t)((uint16_t)attr << 8 | ' ');
    }

    if (y > 0)
        y--;
}

// ============================ char ============================

static void vga_print_char_position(char c, uint32_t x, uint32_t y, uint32_t color)
{
    // проверка границ экрана
    if (x >= VGA_WIDTH || y >= VGA_HEIGHT)
        return;

    uint8_t *vid = VGA_BUF;
    uint8_t fg = rgb_to_vga(color);
    uint8_t attr = make_color(fg, BLACK);

    // вычисляем смещение в байтах
    unsigned int offset = (y * VGA_WIDTH + x) * 2;

    vid[offset] = (uint8_t)c; // ASCII‑код символа
    vid[offset + 1] = attr;   // атрибут цвета
}

static void vga_print_char(char c, uint32_t color)
{
    uint8_t *vid = VGA_BUF;
    uint8_t fg = rgb_to_vga(color);
    uint8_t attr = make_color(fg, BLACK);

    if (c == '\n')
    {
        x = 0;
        y++;
        if (y >= VGA_HEIGHT)
        {
            vga_scroll();
            y = VGA_HEIGHT - 1;
        }
        vga_update_cursor(x, y);
        return;
    }

    // вычисляем смещение в байтах
    unsigned int offset = (y * VGA_WIDTH + x) * 2;

    vid[offset] = (uint8_t)c; // ASCII-код символа
    vid[offset + 1] = attr;   // атрибут цвета

    // двигаем курсор
    x++;
    if (x >= VGA_WIDTH)
    {
        x = 0;
        y++;
        if (y >= VGA_HEIGHT)
        {
            vga_scroll();
            y = VGA_HEIGHT - 1;
        }
    }
    vga_update_cursor(x, y);
}

// ============================ string ============================

static void vga_print_string_position(const char *str, uint32_t x, uint32_t y, uint32_t color)
{
    // проверка границ экрана
    if (!str || x >= VGA_WIDTH || y >= VGA_HEIGHT)
        return;

    uint8_t *vid = VGA_BUF;
    unsigned int offset = (y * VGA_WIDTH + x) * 2;
    uint8_t fg = rgb_to_vga(color);
    uint8_t attr = make_color(fg, BLACK);

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
                vid[offset + 1] = attr;
                offset += 2;
                col++;
            }
        }
        else
        {
            vid[offset] = (uint8_t)c;
            vid[offset + 1] = attr;
            offset += 2;
            col++;
        }

        // если дошли до конца строки VGA
        if (col >= VGA_WIDTH)
            break; // (или можно сделать перенос)
    }
}

static void vga_print_string(const char *str, uint32_t color)
{
    if (!str)
        return;

    for (const char *p = str; *p; ++p)
        vga_print_char(*p, color);
}

static void vga_backspace(void)
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

    vga_update_cursor(x, y);
}

static void vga_update_cursor(uint32_t x, uint32_t y)
{
    /* Обрезаем до размеров VGA */
    if (x >= VGA_WIDTH)
        x = VGA_WIDTH - 1;
    if (y >= VGA_HEIGHT)
        y = VGA_HEIGHT - 1;

    uint16_t pos = (uint16_t)(y * VGA_WIDTH + x);
    // старший байт
    io_write8(VGA_CTRL, CURSOR_HIGH);
    io_write8(VGA_DATA, (uint8_t)((pos >> 8) & 0xFF));
    // младший байт

    io_write8(VGA_CTRL, CURSOR_LOW);
    io_write8(VGA_DATA, (uint8_t)(pos & 0xFF));
}

static void vga_draw_point(uint32_t x, uint32_t y, uint32_t color)
{
    (void)x;
    (void)y;
    (void)color;
}

static void vga_draw_line(int32_t x0, int32_t y0, int32_t x1, int32_t y1, uint32_t color)
{
    (void)x0;
    (void)y0;
    (void)x1;
    (void)y1;
    (void)color;
}

static void vga_draw_circle(int32_t xc, int32_t yc, int32_t r, uint32_t color)
{
    (void)xc;
    (void)yc;
    (void)r;
    (void)color;
}

static void vga_fill_circle(int32_t xc, int32_t yc, int32_t r, uint32_t color)
{
    (void)xc;
    (void)yc;
    (void)r;
    (void)color;
}

static void vga_draw_rect(int32_t x0, int32_t y0, int32_t x1, int32_t y1, uint32_t color)
{
    (void)x0;
    (void)y0;
    (void)x1;
    (void)y1;
    (void)color;
}

static void vga_fill_rect(int32_t x0, int32_t y0, int32_t x1, int32_t y1, uint32_t color)
{
    (void)x0;
    (void)y0;
    (void)x1;
    (void)y1;
    (void)color;
}

const video_ops_t vga_ops = {
    .name = "vga-text",
    .init = NULL, /* не нужна */
    .print_char_position = vga_print_char_position,
    .print_char = vga_print_char,
    .print_string_position = vga_print_string_position,
    .print_string = vga_print_string,
    .backspace = vga_backspace,
    .update_cursor = vga_update_cursor,
    .clean_screen = vga_clean_screen,
    .clear = vga_clear,
    .scroll = vga_scroll,
    .update_screen = NULL, /* прямая запись, буфер не нужен */
    .draw_point = vga_draw_point,
    .draw_line = vga_draw_line,
    .draw_circle = vga_draw_circle,
    .fill_circle = vga_fill_circle,
    .draw_rect = vga_draw_rect,
    .fill_rect = vga_fill_rect,
};

void vga_register(void)
{
    video_register(&vga_ops);
}