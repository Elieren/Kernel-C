#ifndef VGA_H
#define VGA_H

#include <stdint.h>

enum vga_mode {
    TEXT_MODE = 0x03,
    GRAPHICS_MODE = 0x13
};

enum vga_color
{
    BLACK,
    BLUE,
    GREEN,
    CYAN,
    RED,
    MAGENTA,
    BROWN,
    GREY,
    DARK_GREY,
    BRIGHT_BLUE,
    BRIGHT_GREEN,
    BRIGHT_CYAN,
    BRIGHT_RED,
    BRIGHT_MAGENTA,
    YELLOW,
    WHITE,
};

extern enum vga_mode current_mode;

void clean_screen(void);
void print_char_position(const char c,
                         const unsigned int x,
                         const unsigned int y,
                         const uint8_t fore,
                         const uint8_t back);

void print_char(const char c,
                const uint8_t fore,
                const uint8_t back);

void print_string_position(const char *str,
                           const unsigned int x,
                           const unsigned int y,
                           const uint8_t fore,
                           const uint8_t back);

void print_string(const char *str,
                  const uint8_t fore,
                  const uint8_t back);

void backspace(void);

void update_hardware_cursor(uint8_t x, uint8_t y);

void print_time(void);
void print_systemup(void);

void set_vga_mode(enum vga_mode mode);

void draw_pixel(uint16_t x, uint16_t y, uint8_t color);
void clear_screen_graphics(uint8_t color);
void draw_line(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint8_t color);
void draw_rectangle(uint16_t x, uint16_t y, uint16_t width, uint16_t height, uint8_t color);
void draw_filled_rectangle(uint16_t x, uint16_t y, uint16_t width, uint16_t height, uint8_t color);
void draw_circle(uint16_t x, uint16_t y, uint16_t radius, uint8_t color);
void draw_filled_circle(uint16_t x, uint16_t y, uint16_t radius, uint8_t color);

extern const char *prompt;

#endif
