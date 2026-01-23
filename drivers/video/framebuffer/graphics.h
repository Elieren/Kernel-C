#ifndef GRAPHICS_H
#define GRAPHICS_H

#include <stdint.h>
#include <stdbool.h>
#include <boot/bootinfo.h>

typedef struct
{
   uint32_t cols;
   uint32_t rows;
} grid_t;

typedef struct
{
   char ch;        /* код символа / индекс глифа */
   uint32_t color; /* цвет символа (ARGB или ваш формат) */
} cell_t;

typedef struct
{
   uint32_t x, y;
} point_t;

/* Инициализация: передаём указатель на структуру framebuffer_info_t.
   Должна быть вызвана перед всеми остальными функциями. */
void gfx_init(framebuffer_info_t *fb);

void gfx_put_char_position(uint32_t gx, uint32_t gy, char ch, uint32_t color);
void gfx_put_char(char ch, uint32_t color);

void gfx_put_string_position(const char *str,
                             uint32_t gx,
                             uint32_t gy,
                             uint32_t color);
void gfx_put_string(const char *str, uint32_t color);

void gfx_scroll_cells(void);
void gfx_clear_cells(void);
void gfx_backspace(void);

void gfx_draw_all_from_cells(void);

void gfx_update_screen(void);

/* Рисование примитивов.
color — 0x00RRGGBB (или 0xAARRGGBB; при bpp==24/32 используется низкие 3 байта). */
void gfx_draw_point(uint32_t x, uint32_t y, uint32_t color);

/* Линия: Bresenham, поддерживает любые координаты (signed). */
void gfx_draw_line(int32_t x0, int32_t y0, int32_t x1, int32_t y1, uint32_t color);

/* Окружность: outline и заполненная версия */
void gfx_draw_circle(int32_t xc, int32_t yc, int32_t radius, uint32_t color);
void gfx_fill_circle(int32_t xc, int32_t yc, int32_t radius, uint32_t color);

/* Прямоугольники */
void gfx_draw_rect(int32_t x0, int32_t y0, int32_t x1, int32_t y1, uint32_t color);
void gfx_fill_rect(int32_t x0, int32_t y0, int32_t x1, int32_t y1, uint32_t color);

/* Очистка экрана (заполнение цветом) */
void gfx_clear(uint32_t color);

void gfx_draw_glyph(const uint8_t *glyph, int x0, int y0, uint32_t color, int scale);

#endif /* GRAPHICS_H */
