#include "video.h"
#include <stddef.h>

static const video_ops_t *active = NULL;

void video_register(const video_ops_t *ops)
{
    active = ops;
}

/* --------------------------------------------------------------------------- */

void video_init(void)
{
    if (active && active->init)
        active->init();
}

void video_print_char_position(char c, uint32_t x, uint32_t y, uint32_t color)
{
    if (active && active->print_char_position)
        active->print_char_position(c, x, y, color);
}

void video_print_char(char c, uint32_t color)
{
    if (active && active->print_char)
        active->print_char(c, color);
}

void video_print_string_position(const char *str, uint32_t x, uint32_t y, uint32_t color)
{
    if (active && active->print_string_position)
        active->print_string_position(str, x, y, color);
}

void video_print_string(const char *str, uint32_t color)
{
    if (active && active->print_string)
        active->print_string(str, color);
}

void video_backspace(void)
{
    if (active && active->backspace)
        active->backspace();
}

void video_update_cursor(uint32_t x, uint32_t y)
{
    if (active && active->update_cursor)
        active->update_cursor(x, y);
}

void video_clean_screen(void)
{
    if (active && active->clean_screen)
        active->clean_screen();
}

void video_clear(uint32_t color)
{
    if (active && active->clear)
        active->clear(color);
}

void video_scroll(void)
{
    if (active && active->scroll)
        active->scroll();
}

void video_update_screen(void)
{
    if (active && active->update_screen)
        active->update_screen();
}

void video_draw_point(uint32_t x, uint32_t y, uint32_t color)
{
    if (active && active->draw_point)
        active->draw_point(x, y, color);
}

void video_draw_line(int32_t x0, int32_t y0, int32_t x1, int32_t y1, uint32_t color)
{
    if (active && active->draw_line)
        active->draw_line(x0, y0, x1, y1, color);
}

void video_draw_circle(int32_t xc, int32_t yc, int32_t radius, uint32_t color)
{
    if (active && active->draw_circle)
        active->draw_circle(xc, yc, radius, color);
}

void video_fill_circle(int32_t xc, int32_t yc, int32_t radius, uint32_t color)
{
    if (active && active->fill_circle)
        active->fill_circle(xc, yc, radius, color);
}

void video_draw_rect(int32_t x0, int32_t y0, int32_t x1, int32_t y1, uint32_t color)
{
    if (active && active->draw_rect)
        active->draw_rect(x0, y0, x1, y1, color);
}

void video_fill_rect(int32_t x0, int32_t y0, int32_t x1, int32_t y1, uint32_t color)
{
    if (active && active->fill_rect)
        active->fill_rect(x0, y0, x1, y1, color);
}