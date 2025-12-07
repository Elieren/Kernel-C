#include "font.h"
#include <stddef.h>

/* Возвращает глиф для ASCII, или glyph_space если не найден */
const uint8_t *font_get_glyph(char c)
{
    unsigned char uc = (unsigned char)c;
    if (uc < 128 && glyph_table[uc] != NULL)
        return glyph_table[uc];
    return (const uint8_t *)glyph_space;
}

/* Регистрирует/заменяет глиф для ASCII-символа */
void font_register_glyph(char c, const uint8_t *glyph)
{
    unsigned char uc = (unsigned char)c;
    if (uc < 128 && glyph != NULL)
        glyph_table[uc] = glyph;
}

void font_draw_char(char c, int x, int y, uint32_t color, int scale)
{
    if (scale <= 0)
        return;

    const uint8_t *glyph = font_get_glyph(c);
    gfx_draw_glyph(glyph, x, y, color, scale);
}

int font_draw_text(const char *s, int x, int y, uint32_t color, int scale, int spacing)
{
    if (!s || !*s || scale <= 0 || spacing < 0)
    {
        if (!s || scale <= 0)
            return x;
        spacing = 0;
    }

    int xpos = x;
    const char *p = s;

    while (*p)
    {
        if (*p == '\n')
        {
            y += FONT_GLYPH_HEIGHT * scale;
            xpos = x;
        }
        else
        {
            const uint8_t *glyph = font_get_glyph(*p);
            gfx_draw_glyph(glyph, xpos, y, color, scale);
            xpos += FONT_GLYPH_WIDTH * scale + spacing;
        }
        ++p;
    }

    return xpos;
}