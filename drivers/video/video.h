#ifndef VIDEO_H
#define VIDEO_H

#include <stdint.h>

typedef struct
{
    const char *name;

    void (*init)(void);

    /* Текстовый вывод */
    void (*print_char_position)(char c, uint32_t x, uint32_t y, uint32_t color);
    void (*print_char)(char c, uint32_t color);
    void (*print_string_position)(const char *str, uint32_t x, uint32_t y, uint32_t color);
    void (*print_string)(const char *str, uint32_t color);
    void (*backspace)(void);
    void (*update_cursor)(uint32_t x, uint32_t y);

    /* Очистка:
       clean_screen — текстовая очистка: сбрасывает ячейки/курсор
                      (VGA: заполняет пробелами; GFX: gfx_clear_cells)
       clear        — заливка пикселей цветом (VGA: ближайший bg-атрибут;
                      GFX: gfx_clear) */
    void (*clean_screen)(void);
    void (*clear)(uint32_t color);

    /* Служебные */
    void (*scroll)(void);
    void (*update_screen)(void); /* VGA: NULL (прямая запись); GFX: gfx_update_screen */

    /* Примитивы (VGA: заглушки; GFX: полная реализация) */
    void (*draw_point)(uint32_t x, uint32_t y, uint32_t color);
    void (*draw_line)(int32_t x0, int32_t y0, int32_t x1, int32_t y1, uint32_t color);
    void (*draw_circle)(int32_t xc, int32_t yc, int32_t radius, uint32_t color);
    void (*fill_circle)(int32_t xc, int32_t yc, int32_t radius, uint32_t color);
    void (*draw_rect)(int32_t x0, int32_t y0, int32_t x1, int32_t y1, uint32_t color);
    void (*fill_rect)(int32_t x0, int32_t y0, int32_t x1, int32_t y1, uint32_t color);
} video_ops_t;

/* Регистрация активного драйвера */
void video_register(const video_ops_t *ops);

/* Единый API ядра и приложений */
void video_init(void);

void video_print_char_position(char c, uint32_t x, uint32_t y, uint32_t color);
void video_print_char(char c, uint32_t color);
void video_print_string_position(const char *str, uint32_t x, uint32_t y, uint32_t color);
void video_print_string(const char *str, uint32_t color);
void video_backspace(void);
void video_update_cursor(uint32_t x, uint32_t y);

void video_clean_screen(void);
void video_clear(uint32_t color);

void video_scroll(void);
void video_update_screen(void);

void video_draw_point(uint32_t x, uint32_t y, uint32_t color);
void video_draw_line(int32_t x0, int32_t y0, int32_t x1, int32_t y1, uint32_t color);
void video_draw_circle(int32_t xc, int32_t yc, int32_t radius, uint32_t color);
void video_fill_circle(int32_t xc, int32_t yc, int32_t radius, uint32_t color);
void video_draw_rect(int32_t x0, int32_t y0, int32_t x1, int32_t y1, uint32_t color);
void video_fill_rect(int32_t x0, int32_t y0, int32_t x1, int32_t y1, uint32_t color);

#endif /* VIDEO_H */