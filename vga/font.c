#include "font.h"

#include <stddef.h>

/* Возвращает глиф для ASCII, или glyph_space если не найден */
const uint8_t *font_get_glyph(char c)
{
    unsigned char uc = (unsigned char)c;
    if (uc < 128 && glyph_table[uc])
        return glyph_table[uc];
    /* Если для символа нет глифа — возвращаем пробел */
    return (const uint8_t *)glyph_space;
}

/* Регистрирует/заменяет глиф для ASCII-символа */
void font_register_glyph(char c, const uint8_t *glyph)
{
    unsigned char uc = (unsigned char)c;
    if (uc < 128)
        glyph_table[uc] = glyph;
}

void font_draw_char(char c, int x, int y, uint32_t color, int scale)
{
    const uint8_t *glyph = font_get_glyph(c);
    if (!glyph)
        return;
    gfx_draw_glyph(glyph, x, y, color, scale);
}

int font_draw_text(const char *s, int x, int y, uint32_t color, int scale, int spacing)
{
    if (!s)
        return 0;
    if (scale <= 0)
        scale = 1;
    if (spacing < 0)
        spacing = 0;

    const int GLYPH_W = 8;  /* полная ширина глифа */
    const int GLYPH_H = 12; /* полная высота глифа */

    int xpos = x;
    const char *p = s;

    while (*p)
    {
        if (*p == '\n')
        {
            /* переход на следующую строку по полной высоте глифа */
            y += GLYPH_H * scale;
            xpos = x;
            ++p;
            continue;
        }

        const uint8_t *glyph = font_get_glyph(*p);
        if (glyph)
        {
            /* Рисуем глиф целиком в текущей позиции (никакой "left" и "width" не используется) */
            gfx_draw_glyph(glyph, xpos, y, color, scale);
        }

        /* В любом случае сдвигаемся на полную ширину глифа + spacing */
        xpos += GLYPH_W * scale + spacing;
        ++p;
    }

    return xpos;
}