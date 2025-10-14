// vga_graphics.c
#include "vga_graphics.h"
#include <stdint.h>
#include <stddef.h>
#include <string.h>

/* Очищает графический экран цветом (color — палитровый индекс при 8bpp). */
void clear_screen_graphics(uint8_t color)
{
    if (fb_addr == 0)
        return;
    if (fb_bpp == 8)
    {
        for (uint32_t row = 0; row < VGA_GRAPHICS_HEIGHT; ++row)
        {
            memset(VGA_GRAPHICS_BUF + row * fb_pitch, color, VGA_GRAPHICS_WIDTH);
        }
    }
    else if (fb_bpp == 32)
    {
        uint32_t col32 = (uint32_t)color; /* при 32bpp ожидается внешняя упаковка */
        for (uint32_t y = 0; y < VGA_GRAPHICS_HEIGHT; ++y)
        {
            uint8_t *rowptr = VGA_GRAPHICS_BUF + y * fb_pitch;
            for (uint32_t x = 0; x < VGA_GRAPHICS_WIDTH; ++x)
            {
                *(uint32_t *)(rowptr + x * 4) = col32;
            }
        }
    }
    else
    {
        /* fallback: пишем байт за байтом */
        for (uint32_t y = 0; y < VGA_GRAPHICS_HEIGHT; ++y)
            for (uint32_t x = 0; x < VGA_GRAPHICS_WIDTH; ++x)
                VGA_GRAPHICS_BUF[y * fb_pitch + x] = color;
    }
}

/* Рисует один пиксель (color — палитровый индекс для 8bpp). */
void draw_pixel(uint16_t x, uint16_t y, uint8_t color)
{
    if (fb_addr == 0)
        return;
    if ((uint32_t)x >= VGA_GRAPHICS_WIDTH || (uint32_t)y >= VGA_GRAPHICS_HEIGHT)
        return;

    if (fb_bpp == 8)
    {
        VGA_GRAPHICS_BUF[y * fb_pitch + x] = color;
    }
    else if (fb_bpp == 32)
    {
        uint8_t *p = VGA_GRAPHICS_BUF + y * fb_pitch + x * 4;
        /* При 32bpp сюда должен прийти упакованный 32-битный цвет в low bytes через color;
           но для минимальной поддержки запишем повторяющийся байт. */
        uint32_t c32 = ((uint32_t)color) | ((uint32_t)color << 8) | ((uint32_t)color << 16) | ((uint32_t)0xFF << 24);
        *(uint32_t *)p = c32;
    }
    else
    {
        VGA_GRAPHICS_BUF[y * fb_pitch + x] = color;
    }
}

/* Bresenham line */
void draw_line(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint8_t color)
{
    int dx = (int)x2 - (int)x1;
    int dy = (int)y2 - (int)y1;
    int sx = dx >= 0 ? 1 : -1;
    int sy = dy >= 0 ? 1 : -1;
    dx = dx >= 0 ? dx : -dx;
    dy = dy >= 0 ? dy : -dy;
    int err = dx - dy;

    int xi = x1;
    int yi = y1;

    while (1)
    {
        draw_pixel((uint16_t)xi, (uint16_t)yi, color);
        if (xi == (int)x2 && yi == (int)y2)
            break;
        int e2 = err * 2;
        if (e2 > -dy)
        {
            err -= dy;
            xi += sx;
        }
        if (e2 < dx)
        {
            err += dx;
            yi += sy;
        }
    }
}

/* Нарисовать прямоугольник (границы) */
void draw_rectangle(uint16_t x, uint16_t y, uint16_t width, uint16_t height, uint8_t color)
{
    if (width == 0 || height == 0)
        return;
    uint16_t x2 = x + width;
    uint16_t y2 = y + height;
    if (x2 >= VGA_GRAPHICS_WIDTH)
        x2 = VGA_GRAPHICS_WIDTH - 1;
    if (y2 >= VGA_GRAPHICS_HEIGHT)
        y2 = VGA_GRAPHICS_HEIGHT - 1;

    draw_line(x, y, x2, y, color);   /* top */
    draw_line(x2, y, x2, y2, color); /* right */
    draw_line(x2, y2, x, y2, color); /* bottom */
    draw_line(x, y2, x, y, color);   /* left */
}

/* Заполненный прямоугольник */
void draw_filled_rectangle(uint16_t x, uint16_t y, uint16_t width, uint16_t height, uint8_t color)
{
    if (width == 0 || height == 0)
        return;
    uint32_t xend = (uint32_t)x + width;
    uint32_t yend = (uint32_t)y + height;
    if (xend > VGA_GRAPHICS_WIDTH)
        xend = VGA_GRAPHICS_WIDTH;
    if (yend > VGA_GRAPHICS_HEIGHT)
        yend = VGA_GRAPHICS_HEIGHT;

    for (uint32_t yy = y; yy < yend; ++yy)
    {
        uint8_t *row = VGA_GRAPHICS_BUF + yy * fb_pitch;
        if (fb_bpp == 8)
        {
            memset(row + x, color, xend - x);
        }
        else
        {
            for (uint32_t xx = x; xx < xend; ++xx)
                draw_pixel((uint16_t)xx, (uint16_t)yy, color);
        }
    }
}

/* Окружность (outline) — алгоритм средней точки */
void draw_circle(uint16_t x0, uint16_t y0, uint16_t radius, uint8_t color)
{
    int x = (int)radius;
    int y = 0;
    int err = 1 - x;

    while (x >= y)
    {
        if ((uint32_t)(x0 + x) < VGA_GRAPHICS_WIDTH && (uint32_t)(y0 + y) < VGA_GRAPHICS_HEIGHT)
            draw_pixel(x0 + x, y0 + y, color);
        if ((uint32_t)(x0 + y) < VGA_GRAPHICS_WIDTH && (uint32_t)(y0 + x) < VGA_GRAPHICS_HEIGHT)
            draw_pixel(x0 + y, y0 + x, color);
        if ((int)x0 - x >= 0 && (uint32_t)(y0 + y) < VGA_GRAPHICS_HEIGHT)
            draw_pixel(x0 - x, y0 + y, color);
        if ((int)x0 - y >= 0 && (uint32_t)(y0 + x) < VGA_GRAPHICS_HEIGHT)
            draw_pixel(x0 - y, y0 + x, color);
        if ((int)x0 - x >= 0 && (int)y0 - y >= 0)
            draw_pixel(x0 - x, y0 - y, color);
        if ((int)x0 - y >= 0 && (int)y0 - x >= 0)
            draw_pixel(x0 - y, y0 - x, color);
        if ((uint32_t)(x0 + y) < VGA_GRAPHICS_WIDTH && (int)y0 - x >= 0)
            draw_pixel(x0 + y, y0 - x, color);
        if ((uint32_t)(x0 + x) < VGA_GRAPHICS_WIDTH && (int)y0 - y >= 0)
            draw_pixel(x0 + x, y0 - y, color);

        y++;
        if (err < 0)
        {
            err += 2 * y + 1;
        }
        else
        {
            x--;
            err += 2 * (y - x) + 1;
        }
    }
}

/* Заполненная окружность */
void draw_filled_circle(uint16_t x0, uint16_t y0, uint16_t radius, uint8_t color)
{
    int x = (int)radius;
    int y = 0;
    int err = 1 - x;

    while (x >= y)
    {
        /* рисуем горизонтальные линии между симметричными точками */
        int xa = (int)x0 - x;
        int xb = (int)x0 + x;
        int ya = (int)y0 + y;
        int yb = (int)y0 - y;
        if (ya >= 0 && (uint32_t)ya < VGA_GRAPHICS_HEIGHT)
        {
            for (int xi = xa; xi <= xb; ++xi)
                if (xi >= 0 && (uint32_t)xi < VGA_GRAPHICS_WIDTH)
                    draw_pixel((uint16_t)xi, (uint16_t)ya, color);
        }
        if (yb >= 0 && (uint32_t)yb < VGA_GRAPHICS_HEIGHT)
        {
            for (int xi = xa; xi <= xb; ++xi)
                if (xi >= 0 && (uint32_t)xi < VGA_GRAPHICS_WIDTH)
                    draw_pixel((uint16_t)xi, (uint16_t)yb, color);
        }

        xa = (int)x0 - y;
        xb = (int)x0 + y;
        ya = (int)y0 + x;
        yb = (int)y0 - x;
        if (ya >= 0 && (uint32_t)ya < VGA_GRAPHICS_HEIGHT)
        {
            for (int xi = xa; xi <= xb; ++xi)
                if (xi >= 0 && (uint32_t)xi < VGA_GRAPHICS_WIDTH)
                    draw_pixel((uint16_t)xi, (uint16_t)ya, color);
        }
        if (yb >= 0 && (uint32_t)yb < VGA_GRAPHICS_HEIGHT)
        {
            for (int xi = xa; xi <= xb; ++xi)
                if (xi >= 0 && (uint32_t)xi < VGA_GRAPHICS_WIDTH)
                    draw_pixel((uint16_t)xi, (uint16_t)yb, color);
        }

        y++;
        if (err < 0)
        {
            err += 2 * y + 1;
        }
        else
        {
            x--;
            err += 2 * (y - x) + 1;
        }
    }
}

/* Вспомогательная целочисленная конвертация HSV->RGB
   h: 0..359, s: 0..255, v: 0..255
   без плавающей точки */
static void hsv_to_rgb_int(uint16_t h, uint8_t s, uint8_t v, uint8_t *r, uint8_t *g, uint8_t *b)
{
    if (s == 0)
    {
        *r = *g = *b = v;
        return;
    }

    uint16_t region = h / 60;                 /* 0..5 */
    uint16_t remainder = (h % 60) * 255 / 60; /* 0..255 */

    uint16_t p = (uint16_t)((uint32_t)v * (255 - s) / 255);
    uint16_t q = (uint16_t)((uint32_t)v * (255 - (uint32_t)s * remainder / 255) / 255);
    uint16_t t = (uint16_t)((uint32_t)v * (255 - (uint32_t)s * (255 - remainder) / 255) / 255);

    switch (region)
    {
    case 0:
        *r = v;
        *g = (uint8_t)t;
        *b = (uint8_t)p;
        break;
    case 1:
        *r = (uint8_t)q;
        *g = v;
        *b = (uint8_t)p;
        break;
    case 2:
        *r = (uint8_t)p;
        *g = v;
        *b = (uint8_t)t;
        break;
    case 3:
        *r = (uint8_t)p;
        *g = (uint8_t)q;
        *b = v;
        break;
    case 4:
        *r = (uint8_t)t;
        *g = (uint8_t)p;
        *b = v;
        break;
    default:
        *r = v;
        *g = (uint8_t)p;
        *b = (uint8_t)q;
        break;
    }
}

/* Небольшая утилита — перевод RGB -> индекс палитры 0..255 (грубая аппроксимация),
   годится когда у нас 8bpp и нет настроенной палитры. */
static uint8_t rgb_to_index_approx(uint8_t r, uint8_t g, uint8_t b)
{
    /* Преобразуем в яркость + цветовую компоненту для видимой градации.
       Это простая эвристика: используем 6x6x6 куб примерно. */
    uint8_t r6 = (r * 5) / 255;
    uint8_t g6 = (g * 5) / 255;
    uint8_t b6 = (b * 5) / 255;
    return (uint8_t)(16 + (36 * r6) + (6 * g6) + b6);
}

/* Основная функция: рисует полноэкранную радугу слева направо.
   Для 8bpp использует draw_pixel (передавая индекс),
   для 16/24/32 bpp пишет напрямую в framebuffer. */
void draw_rainbow_gradient(void)
{
    if (fb_addr == 0 || fb_width == 0 || fb_height == 0)
        return;

    /* насыщенность и значение (яркость) фиксируем на максимум */
    const uint8_t S = 255;
    const uint8_t V = 255;

    for (uint32_t x = 0; x < fb_width; ++x)
    {
        /* hue: 0..359 по ширине */
        uint16_t hue = (uint32_t)x * 360 / (fb_width ? fb_width : 1);
        uint8_t r, g, b;
        hsv_to_rgb_int(hue % 360, S, V, &r, &g, &b);

        /* 8bpp: используем draw_pixel и приблизительный индекс */
        if (fb_bpp == 8)
        {
            uint8_t idx = rgb_to_index_approx(r, g, b);
            for (uint32_t y = 0; y < fb_height; ++y)
            {
                draw_pixel((uint16_t)x, (uint16_t)y, idx);
            }
            continue;
        }

        /* 16/24/32 bpp — пишем напрямую. Предполагаем little-endian and common layouts:
           - 32bpp: 4 bytes per pixel (0xAARRGGBB) или (B G R A) in memory depending on platform.
             Здесь запишем 0xFFRRGGBB (alpha 0xFF) — это работает на большинстве реал-world fb.
           - 24bpp: 3 bytes per pixel (B,G,R)
           - 16bpp: используем RGB565 (если fb реально другой — надо читать маски из mb2 tag) */
        if (fb_bpp == 32)
        {
            uint32_t color32 = (0xFFu << 24) | ((uint32_t)r << 16) | ((uint32_t)g << 8) | (uint32_t)b;
            for (uint32_t y = 0; y < fb_height; ++y)
            {
                uint8_t *p = VGA_GRAPHICS_BUF + y * fb_pitch + x * 4;
                *(uint32_t *)p = color32;
            }
        }
        else if (fb_bpp == 24)
        {
            for (uint32_t y = 0; y < fb_height; ++y)
            {
                uint8_t *p = VGA_GRAPHICS_BUF + y * fb_pitch + x * 3;
                p[0] = b;
                p[1] = g;
                p[2] = r;
            }
        }
        else if (fb_bpp == 16)
        {
            /* RGB565 */
            uint16_t r5 = (r >> 3) & 0x1F;
            uint16_t g6 = (g >> 2) & 0x3F;
            uint16_t b5 = (b >> 3) & 0x1F;
            uint16_t pix = (uint16_t)((r5 << 11) | (g6 << 5) | b5);
            for (uint32_t y = 0; y < fb_height; ++y)
            {
                uint8_t *p = VGA_GRAPHICS_BUF + y * fb_pitch + x * 2;
                *(uint16_t *)p = pix;
            }
        }
        else
        {
            /* fallback: используем draw_pixel с усреднённым индекс */
            uint8_t idx = (uint8_t)(((uint32_t)r + g + b) / 3);
            for (uint32_t y = 0; y < fb_height; ++y)
            {
                draw_pixel((uint16_t)x, (uint16_t)y, idx);
            }
        }
    }
}