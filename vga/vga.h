#ifndef VGA_H
#define VGA_H

#include <stdint.h>

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

void clean_screen(void);
void print_char(const char c,
                const unsigned int x,
                const unsigned int y,
                const uint8_t fore,
                const uint8_t back);

void print_string(const char *str,
                  const unsigned int x,
                  const unsigned int y,
                  const uint8_t fore,
                  const uint8_t back);

void update_hardware_cursor(uint8_t x, uint8_t y);

void print_time(void);
void print_systemup(void);

extern const char *prompt;

#endif
