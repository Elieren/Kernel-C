#include "graphics.h"
#include <stddef.h>
#include <string.h>

/* Внутренняя глобальная ссылка на информацию о framebuffer */
static framebuffer_info_t *g_fb = NULL;

/* helper: при identity-map физический == виртуальный */
static inline void *phys_to_virt(uint64_t phys)
{
    return (void *)(uintptr_t)phys;
}

void gfx_init(framebuffer_info_t *fb)
{
    g_fb = fb;
}

/* проверка границ */
static inline bool in_bounds(int32_t x, int32_t y)
{
    if (!g_fb)
        return false;
    if (x < 0 || y < 0)
        return false;
    if ((uint32_t)x >= g_fb->width || (uint32_t)y >= g_fb->height)
        return false;
    return true;
}

/* основной writer: поддерживает 24 и 32 bpp; color — 0x00RRGGBB */
void gfx_put_pixel(uint32_t x, uint32_t y, uint32_t color)
{
    if (!g_fb)
        return;
    if (x >= g_fb->width || y >= g_fb->height)
        return;

    uint8_t *base = (uint8_t *)phys_to_virt(g_fb->addr);
    uint64_t offset = (uint64_t)y * g_fb->pitch + (uint64_t)x * (g_fb->bpp / 8);
    uint8_t *ptr = base + offset;

    if (g_fb->bpp == 32)
    {
        /* записываем 32-битное слово */
        uint32_t *dst = (uint32_t *)ptr;
        *dst = color;
    }
    else if (g_fb->bpp == 24)
    {
        /* little-endian: 3 байта */
        ptr[0] = (uint8_t)(color & 0xFF);
        ptr[1] = (uint8_t)((color >> 8) & 0xFF);
        ptr[2] = (uint8_t)((color >> 16) & 0xFF);
    }
    else
    {
        /* прочие форматы — не поддерживаются в этой версии */
    }
}

void gfx_draw_point(uint32_t x, uint32_t y, uint32_t color)
{
    gfx_put_pixel(x, y, color);
}

/* Алгоритм Брезенхэма для линий (работает в любом направлении) */
void gfx_draw_line(int32_t x0, int32_t y0, int32_t x1, int32_t y1, uint32_t color)
{
    if (!g_fb)
        return;

    int32_t dx = x1 - x0;
    int32_t dy = y1 - y0;

    int32_t sx = (dx >= 0) ? 1 : -1;
    int32_t sy = (dy >= 0) ? 1 : -1;

    int32_t ax = dx >= 0 ? dx : -dx;
    int32_t ay = dy >= 0 ? dy : -dy;

    int32_t x = x0;
    int32_t y = y0;

    if (ax > ay)
    {
        int32_t d = 2 * ay - ax;
        for (int i = 0; i <= ax; ++i)
        {
            if (in_bounds(x, y))
                gfx_put_pixel((uint32_t)x, (uint32_t)y, color);
            if (d >= 0)
            {
                y += sy;
                d -= 2 * ax;
            }
            x += sx;
            d += 2 * ay;
        }
    }
    else
    {
        int32_t d = 2 * ax - ay;
        for (int i = 0; i <= ay; ++i)
        {
            if (in_bounds(x, y))
                gfx_put_pixel((uint32_t)x, (uint32_t)y, color);
            if (d >= 0)
            {
                x += sx;
                d -= 2 * ay;
            }
            y += sy;
            d += 2 * ax;
        }
    }
}

/* helper: рисует горизонтальную линию [x0..x1] на y (обрезается по экрану) */
static void draw_hline_clipped(int32_t x0, int32_t x1, int32_t y, uint32_t color)
{
    if (!g_fb)
        return;
    if (y < 0 || (uint32_t)y >= g_fb->height)
        return;
    if (x0 > x1)
    {
        int32_t tmp = x0;
        x0 = x1;
        x1 = tmp;
    }
    if (x1 < 0 || (uint32_t)x0 >= g_fb->width)
        return;
    if (x0 < 0)
        x0 = 0;
    if ((uint32_t)x1 >= g_fb->width)
        x1 = (int32_t)g_fb->width - 1;

    uint8_t *base = (uint8_t *)phys_to_virt(g_fb->addr);
    uint64_t row_offset = (uint64_t)y * g_fb->pitch;

    if (g_fb->bpp == 32)
    {
        uint32_t *dst = (uint32_t *)(base + row_offset + (uint64_t)x0 * 4);
        uint32_t count = (uint32_t)(x1 - x0 + 1);
        for (uint32_t i = 0; i < count; ++i)
            dst[i] = color;
    }
    else if (g_fb->bpp == 24)
    {
        for (int32_t x = x0; x <= x1; ++x)
        {
            uint8_t *ptr = base + row_offset + (uint64_t)x * 3;
            ptr[0] = (uint8_t)(color & 0xFF);
            ptr[1] = (uint8_t)((color >> 8) & 0xFF);
            ptr[2] = (uint8_t)((color >> 16) & 0xFF);
        }
    }
}

/* Алгоритм средней точки — контур окружности */
void gfx_draw_circle(int32_t xc, int32_t yc, int32_t radius, uint32_t color)
{
    if (!g_fb)
        return;
    if (radius < 0)
        return;
    int32_t x = 0;
    int32_t y = radius;
    int32_t d = 1 - radius;

    while (x <= y)
    {
        /* симметрия в 8 точках */
        if (in_bounds(xc + x, yc + y))
            gfx_put_pixel((uint32_t)(xc + x), (uint32_t)(yc + y), color);
        if (in_bounds(xc - x, yc + y))
            gfx_put_pixel((uint32_t)(xc - x), (uint32_t)(yc + y), color);
        if (in_bounds(xc + x, yc - y))
            gfx_put_pixel((uint32_t)(xc + x), (uint32_t)(yc - y), color);
        if (in_bounds(xc - x, yc - y))
            gfx_put_pixel((uint32_t)(xc - x), (uint32_t)(yc - y), color);
        if (in_bounds(xc + y, yc + x))
            gfx_put_pixel((uint32_t)(xc + y), (uint32_t)(yc + x), color);
        if (in_bounds(xc - y, yc + x))
            gfx_put_pixel((uint32_t)(xc - y), (uint32_t)(yc + x), color);
        if (in_bounds(xc + y, yc - x))
            gfx_put_pixel((uint32_t)(xc + y), (uint32_t)(yc - x), color);
        if (in_bounds(xc - y, yc - x))
            gfx_put_pixel((uint32_t)(xc - y), (uint32_t)(yc - x), color);

        x++;
        if (d < 0)
        {
            d += 2 * x + 1;
        }
        else
        {
            y--;
            d += 2 * (x - y) + 1;
        }
    }
}

/* Алгоритм средней точки — заполненная окружность: рисует горизонтальные отрезки между симметричными точками */
void gfx_fill_circle(int32_t xc, int32_t yc, int32_t radius, uint32_t color)
{
    if (!g_fb)
        return;
    if (radius < 0)
        return;
    int32_t x = 0;
    int32_t y = radius;
    int32_t d = 1 - radius;

    while (x <= y)
    {
        /* рисуем горизонтальные отрезки между симметричными точками */
        draw_hline_clipped(xc - x, xc + x, yc + y, color);
        draw_hline_clipped(xc - x, xc + x, yc - y, color);
        draw_hline_clipped(xc - y, xc + y, yc + x, color);
        draw_hline_clipped(xc - y, xc + y, yc - x, color);

        x++;
        if (d < 0)
        {
            d += 2 * x + 1;
        }
        else
        {
            y--;
            d += 2 * (x - y) + 1;
        }
    }
}

/* заполнить весь экран заданным цветом */
void gfx_clear(uint32_t color)
{
    if (!g_fb)
        return;
    uint32_t h = g_fb->height;
    for (uint32_t y = 0; y < h; ++y)
    {
        draw_hline_clipped(0, (int32_t)g_fb->width - 1, (int32_t)y, color);
    }
}
