/* vga_graphics.h */
#ifndef VGA_GRAPHICS_H
#define VGA_GRAPHICS_H

#include <stdint.h>
#include <stddef.h>

extern uintptr_t fb_addr;
extern uint32_t fb_width;
extern uint32_t fb_height;
extern uint32_t fb_pitch;
extern uint8_t fb_bpp;

#define VGA_GRAPHICS_BUF ((uint8_t *)fb_addr)
#define VGA_GRAPHICS_WIDTH (fb_width)
#define VGA_GRAPHICS_HEIGHT (fb_height)

void clear_screen_graphics(uint8_t color);

void draw_pixel(uint16_t x, uint16_t y, uint8_t color);
void draw_line(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint8_t color);
void draw_rectangle(uint16_t x, uint16_t y, uint16_t width, uint16_t height, uint8_t color);
void draw_filled_rectangle(uint16_t x, uint16_t y, uint16_t width, uint16_t height, uint8_t color);
void draw_circle(uint16_t x0, uint16_t y0, uint16_t radius, uint8_t color);
void draw_filled_circle(uint16_t x0, uint16_t y0, uint16_t radius, uint8_t color);

/* Рисует горизонтальную переливающуюся радугу слева направо */
void draw_rainbow_gradient(void);

#endif
