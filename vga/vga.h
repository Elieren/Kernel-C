#ifndef VGA_H
#define VGA_H

typedef unsigned char uint8_t;
typedef unsigned char uint8;

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

#endif
