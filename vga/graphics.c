#include "graphics.h"
#include <stddef.h>
#include <string.h>
#include "../malloc/malloc.h"
#include "font.h"

const int GLYPH_W = 8;
const int GLYPH_H = 12;

/* Внутренняя глобальная ссылка на информацию о framebuffer */
static framebuffer_info_t *g_fb = NULL;

/* двойной буфер: back (заполняемый) и front (выводимый) */
static cell_t *g_back_cells = NULL;
static cell_t *g_front_cells = NULL;

/* общее число клеток (для одного буфера) */
static uint64_t g_total_cells = 0;
static grid_t g_grid = {0, 0};

/* курсор в клетках (используется gfx_put_string и backspace) — относится к back buffer */
static uint32_t g_cx = 0;
static uint32_t g_cy = 0;

#define TAB_SIZE 8

/* --------------------------------------------------------------------------- */

/* helper: при identity-map физический == виртуальный */
static inline void *phys_to_virt(uint64_t phys)
{
    return (void *)(uintptr_t)phys;
}

static inline point_t glyph_pixel_pos(uint32_t gx, uint32_t gy,
                                      uint32_t Sw, uint32_t Sh,
                                      uint32_t M, uint32_t scale)
{
    if (scale == 0)
        scale = 1;
    uint64_t step_x = (uint64_t)(GLYPH_W + Sw) * scale;
    uint64_t step_y = (uint64_t)(GLYPH_H + Sh) * scale;
    point_t p;
    p.x = (uint32_t)((uint64_t)M + (uint64_t)gx * step_x);
    p.y = (uint32_t)((uint64_t)M + (uint64_t)gy * step_y);
    return p;
}

/* Индекс по координатам клетки (gx,gy) */
static inline uint64_t cell_index(uint32_t gx, uint32_t gy)
{
    return (uint64_t)gy * (uint64_t)g_grid.cols + (uint64_t)gx;
}

grid_t calc_grid(uint32_t Sw, uint32_t Sh,
                 uint32_t M)
{
    grid_t g;

    uint32_t avail_w = g_fb->width - 2 * M;
    uint32_t avail_h = g_fb->height - 2 * M;

    g.cols = (avail_w + Sw) / (GLYPH_W + Sw);
    g.rows = (avail_h + Sh) / (GLYPH_H + Sh);

    return g;
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

/* --------------------------------------------------------------------------- */

void gfx_init(framebuffer_info_t *fb)
{
    g_fb = fb;

    g_grid = calc_grid(2, 2, 10);

    if (g_grid.cols == 0 || g_grid.rows == 0)
        return;

    /* проверка переполнения при умножении */
    uint64_t total = (uint64_t)g_grid.cols * (uint64_t)g_grid.rows;
    if (total == 0)
    {
        return;
    }

    size_t need = (size_t)(total * sizeof(cell_t));

    /* аллоцируем два буфера подряд: back + front */
    cell_t *buf = (cell_t *)malloc(need * 2);
    if (!buf)
        return;

    /* очистим оба буфера (по умолчанию пробел и цвет 0) */
    memset(buf, 0, need * 2);

    /* сохраняем в глобальные переменные */
    g_back_cells = buf;
    g_front_cells = buf + total;
    g_total_cells = total;
}

// ============================ char ============================

void gfx_put_char_position(uint32_t gx, uint32_t gy, char ch, uint32_t color)
{
    if (!g_back_cells)
        return;
    if (gx >= g_grid.cols || gy >= g_grid.rows)
        return;

    uint64_t idx = cell_index(gx, gy);
    g_back_cells[idx].ch = ch;
    g_back_cells[idx].color = color;
}

void gfx_put_char(char ch, uint32_t color)
{
    if (!g_back_cells)
        return;
    if (g_grid.cols == 0 || g_grid.rows == 0)
        return;

    /* newline: перейти на начало следующей строки */
    if (ch == '\n')
    {
        g_cx = 0;
        g_cy++;
        if (g_cy >= g_grid.rows)
        {
            gfx_scroll_cells();

            g_cy = g_grid.rows - 1; /* остаёмся на последней строке */
        }
        return;
    }

    /* записать символ в текущую позицию (в back buffer) */
    if (g_cx < g_grid.cols && g_cy < g_grid.rows)
    {
        uint64_t idx = cell_index(g_cx, g_cy);
        g_back_cells[idx].ch = ch;
        g_back_cells[idx].color = color;
    }
    else
    {
        /* вне экрана - ничего не делаем */
        return;
    }

    g_cx++;
    if (g_cx >= g_grid.cols)
    {
        g_cx = 0;
        g_cy++;
        if (g_cy >= g_grid.rows)
        {
            gfx_scroll_cells();
            g_cy = g_grid.rows - 1;
        }
    }

    // gfx_update_cursor(g_cx, g_cy);
}

// ============================ string ============================

void gfx_put_string_position(const char *str,
                             uint32_t gx,
                             uint32_t gy,
                             uint32_t color)
{
    if (!g_back_cells || !str)
        return;
    if (g_grid.cols == 0 || g_grid.rows == 0)
        return;
    if (gx >= g_grid.cols || gy >= g_grid.rows)
        return;

    uint32_t col = gx;
    uint32_t row = gy;
    uint32_t cols = g_grid.cols;

    for (size_t i = 0; str[i]; ++i)
    {
        char c = str[i];

        if (c == '\t')
        {
            /* сколько пробелов до следующего кратного TAB_SIZE */
            uint32_t spaces = TAB_SIZE - (col % TAB_SIZE);
            for (uint32_t s = 0; s < spaces; ++s)
            {
                if (col >= cols)
                    break;
                uint64_t idx = cell_index(col, row);
                g_back_cells[idx].ch = ' ';
                g_back_cells[idx].color = color;
                col++;
            }
        }
        else
        {
            if (col >= cols)
                break;
            uint64_t idx = cell_index(col, row);
            g_back_cells[idx].ch = c;
            g_back_cells[idx].color = color;
            col++;
        }

        if (col >= cols)
            break;
    }
}

void gfx_put_string(const char *str, uint32_t color)
{
    if (!g_back_cells || !str)
        return;
    if (g_grid.cols == 0 || g_grid.rows == 0)
        return;

    for (size_t i = 0; str[i]; ++i)
    {
        gfx_put_char(str[i], color);
    }

    // gfx_update_cursor(g_cx, g_cy);
}

// =================================================================

void gfx_scroll_cells(void)
{
    if (!g_back_cells || g_grid.rows == 0 || g_grid.cols == 0)
        return;

    uint32_t cols = g_grid.cols;
    uint32_t rows = g_grid.rows;

    /* сдвиг вверх */
    for (uint32_t gy = 0; gy + 1 < rows; ++gy)
    {
        for (uint32_t gx = 0; gx < cols; ++gx)
        {
            uint64_t dst = cell_index(gx, gy);
            uint64_t src = cell_index(gx, gy + 1);
            g_back_cells[dst] = g_back_cells[src];
        }
    }

    /* очистить последнюю строку */
    uint32_t last = rows - 1;
    for (uint32_t gx = 0; gx < cols; ++gx)
    {
        uint64_t idx = cell_index(gx, last);
        g_back_cells[idx].ch = 0;
        g_back_cells[idx].color = 0;
    }

    /* поправим курсор, если он был в видимой области */
    if (g_cy > 0)
    {
        --g_cy;
    }
    else
    {
        g_cx = 0;
    }
}

/* Очистить все клетки (символ=0, цвет=0) и сбросить курсор — очищаем back buffer */
void gfx_clear_cells(void)
{
    if (!g_back_cells || g_total_cells == 0)
        return;

    memset(g_back_cells, 0, (size_t)g_total_cells * sizeof(cell_t));
    g_cx = 0;
    g_cy = 0;
}

void gfx_backspace(void)
{
    if (!g_back_cells || g_grid.cols == 0 || g_grid.rows == 0)
        return;

    if (g_cx == 0)
    {
        if (g_cy > 0)
        {
            g_cy--;
            g_cx = g_grid.cols - 1;
        }
        else
        {
            /* в самом начале - ничего стереть */
            return;
        }
    }
    else
    {
        g_cx--;
    }

    /* очистить текущую позицию в back buffer */
    uint64_t idx = cell_index(g_cx, g_cy);
    g_back_cells[idx].ch = 0;
    g_back_cells[idx].color = 0;
}

/* Рисуем весь front buffer на экран */
void gfx_draw_all_from_cells(void)
{
    if (!g_front_cells)
        return;

    for (uint32_t gy = 0; gy < g_grid.rows; ++gy)
    {
        for (uint32_t gx = 0; gx < g_grid.cols; ++gx)
        {
            uint64_t idx = cell_index(gx, gy);
            char ch = g_front_cells[idx].ch;
            uint32_t color = g_front_cells[idx].color;
            if (ch == 0)
                continue; /* пустая клетка */
            point_t p = glyph_pixel_pos(gx, gy, /*Sw*/ 2, /*Sh*/ 2, /*M*/ 10, /*scale*/ 1);
            const uint8_t *glyph = font_get_glyph(ch);
            gfx_draw_glyph(glyph, p.x, p.y, color, 1);
        }
    }
}

/* helper — рисует одну клетку (фон или глиф) */
static inline void draw_cell_at(uint32_t gx, uint32_t gy, const cell_t *c)
{
    point_t p = glyph_pixel_pos(gx, gy, /*Sw*/ 2, /*Sh*/ 2, /*M*/ 10, /*scale*/ 1);

    const uint8_t *glyph = font_get_glyph(c->ch);
    gfx_draw_glyph(glyph, p.x, p.y, c->color, 1);
}

/* новая реализация: только изменённые клетки */
void gfx_present_if_changed(void)
{
    if (!g_back_cells || !g_front_cells)
        return;

    uint32_t cols = g_grid.cols;
    if (cols == 0)
        return;

    /* Проходим по всем клеткам и рисуем только те, что отличаются */
    for (uint64_t i = 0; i < g_total_cells; ++i)
    {
        cell_t *b = &g_back_cells[i];
        cell_t *f = &g_front_cells[i];

        if (b->ch != f->ch || b->color != f->color)
        {
            uint32_t gx = (uint32_t)(i % cols);
            uint32_t gy = (uint32_t)(i / cols);

            draw_cell_at(gx, gy, b);

            /* обновляем front — делаем это сразу, чтобы в следующем кадре не перерисовывать */
            f->ch = b->ch;
            f->color = b->color;
        }
    }
}

/* Сразу показать без сравнения */
void gfx_present_force(void)
{
    if (!g_back_cells || !g_front_cells)
        return;

    size_t bytes = (size_t)g_total_cells * sizeof(cell_t);
    memcpy(g_front_cells, g_back_cells, bytes);
    gfx_clear(0x00000000);
    gfx_draw_all_from_cells();
}

/* --------------------------------------------------------------------------- */

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

/* Рисует контур прямоугольника (x0,y0)-(x1,y1) */
void gfx_draw_rect(int32_t x0, int32_t y0, int32_t x1, int32_t y1, uint32_t color)
{
    if (!g_fb)
        return;

    /* нормализуем координаты */
    if (x0 > x1)
    {
        int32_t t = x0;
        x0 = x1;
        x1 = t;
    }
    if (y0 > y1)
    {
        int32_t t = y0;
        y0 = y1;
        y1 = t;
    }

    /* если высота нулевая — рисуем горизонтальную линию */
    if (y0 == y1)
    {
        draw_hline_clipped(x0, x1, y0, color);
        return;
    }

    /* если ширина нулевая — рисуем вертикальную линию */
    if (x0 == x1)
    {
        for (int32_t y = y0; y <= y1; ++y)
            if (in_bounds(x0, y))
                gfx_put_pixel((uint32_t)x0, (uint32_t)y, color);
        return;
    }

    /* верх и низ */
    draw_hline_clipped(x0, x1, y0, color);
    draw_hline_clipped(x0, x1, y1, color);

    /* левый и правый края (по пикселю) */
    for (int32_t y = y0; y <= y1; ++y)
    {
        if (in_bounds(x0, y))
            gfx_put_pixel((uint32_t)x0, (uint32_t)y, color);
        if (in_bounds(x1, y))
            gfx_put_pixel((uint32_t)x1, (uint32_t)y, color);
    }
}

/* Заполняет прямоугольник (x0,y0)-(x1,y1) цветом */
void gfx_fill_rect(int32_t x0, int32_t y0, int32_t x1, int32_t y1, uint32_t color)
{
    if (!g_fb)
        return;

    /* нормализуем координаты */
    if (x0 > x1)
    {
        int32_t t = x0;
        x0 = x1;
        x1 = t;
    }
    if (y0 > y1)
    {
        int32_t t = y0;
        y0 = y1;
        y1 = t;
    }

    /* перебираем строки и рисуем отрезки (draw_hline_clipped сам обрежет по X) */
    for (int32_t y = y0; y <= y1; ++y)
    {
        draw_hline_clipped(x0, x1, y, color);
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

/* --------------------------------------------------------------------------- */

void gfx_draw_glyph(const uint8_t *glyph, int x0, int y0, uint32_t color, int scale)
{
    if (!glyph)
        return;
    if (scale <= 0)
        scale = 1;

    if (scale == 1)
    {
        /* быстрый путь для scale == 1 */
        for (int row = 0; row < 12; row++)
        {
            uint8_t line = glyph[row];
            for (int col = 0; col < 8; col++)
            {
                if (line & (1u << (7 - col)))
                {
                    gfx_put_pixel((uint32_t)(x0 + col), (uint32_t)(y0 + row), color);
                }
            }
        }
    }
    else
    {
        /* масштабируем каждый включённый пиксель в квадрат scale x scale */
        for (int row = 0; row < 12; row++)
        {
            uint8_t line = glyph[row];
            for (int col = 0; col < 8; col++)
            {
                if (line & (1u << (7 - col)))
                {
                    int base_x = x0 + col * scale;
                    int base_y = y0 + row * scale;
                    for (int dy = 0; dy < scale; ++dy)
                    {
                        for (int dx = 0; dx < scale; ++dx)
                        {
                            gfx_put_pixel((uint32_t)(base_x + dx), (uint32_t)(base_y + dy), color);
                        }
                    }
                }
            }
        }
    }
}
