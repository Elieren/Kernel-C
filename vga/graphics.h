#ifndef GRAPHICS_H
#define GRAPHICS_H

#include <stdint.h>
#include <stdbool.h>
#include "mb2/mb2.h"

/* Инициализация: передаём указатель на структуру framebuffer_info_t.
   Должна быть вызвана перед всеми остальными функциями. */
void gfx_init(framebuffer_info_t *fb);

/* Рисование примитивов.
   color — 0x00RRGGBB (или 0xAARRGGBB; при bpp==24/32 используется низкие 3 байта). */
void gfx_put_pixel(uint32_t x, uint32_t y, uint32_t color);
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
