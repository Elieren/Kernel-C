#ifndef GRAPHICS_H
#define GRAPHICS_H

#include <stdint.h>
#include <stdbool.h>
#include <boot/bootinfo.h>
#include "drivers/video/video.h"

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

void gfx_draw_glyph(const uint8_t *glyph, int x0, int y0, uint32_t color, int scale);

#endif /* GRAPHICS_H */
