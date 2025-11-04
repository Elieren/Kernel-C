#ifndef SYMBOLS_GLYPH_H
#define SYMBOLS_GLYPH_H

#include <stdint.h>

/* glyph_space 8x8 */
static const uint8_t glyph_space[8][1] = {
    {0x00},
    {0x00},
    {0x00},
    {0x00},
    {0x00},
    {0x00},
    {0x00},
    {0x00},
};

/* glyph_! 8x8 */
static const uint8_t glyph_exclamation_mark[8][1] = {
    {0x80},
    {0x80},
    {0x80},
    {0x80},
    {0x80},
    {0x80},
    {0x00},
    {0x80},
};

/* glyph_? 8x8 */
static const uint8_t glyph_question_mark[8][1] = {
    {0x40},
    {0xA0},
    {0x20},
    {0x20},
    {0x40},
    {0x40},
    {0x00},
    {0x40},
};

/* glyph_$ 8x8 */
static const uint8_t glyph_dollar[8][1] = {
    {0x00},
    {0x20},
    {0x70},
    {0x80},
    {0x70},
    {0x08},
    {0x70},
    {0x20},
};

/* glyph_: 8x8 */
static const uint8_t glyph_colon[8][1] = {
    {0x00},
    {0x00},
    {0x80},
    {0x00},
    {0x00},
    {0x00},
    {0x80},
    {0x00},
};

#endif