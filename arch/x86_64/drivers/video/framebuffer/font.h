#ifndef FONT_H
#define FONT_H

#include <stdint.h>

#include "graphics.h"

#include "lib/graphics/glyphs/english_glyph.h"
#include "lib/graphics/glyphs/numbers_glyph.h"
#include "lib/graphics/glyphs/symbols_glyph.h"

/* Константы размеров глифов */
#define FONT_GLYPH_WIDTH 8
#define FONT_GLYPH_HEIGHT 12

#ifdef __GNUC__
#define UNUSED __attribute__((unused))
#else
#define UNUSED
#endif

static const uint8_t *glyph_table[128] UNUSED = {
    /* Upper */
    ['A'] = (const uint8_t *)glyph_A,
    ['B'] = (const uint8_t *)glyph_B,
    ['C'] = (const uint8_t *)glyph_C,
    ['D'] = (const uint8_t *)glyph_D,
    ['E'] = (const uint8_t *)glyph_E,
    ['F'] = (const uint8_t *)glyph_F,
    ['G'] = (const uint8_t *)glyph_G,
    ['H'] = (const uint8_t *)glyph_H,
    ['I'] = (const uint8_t *)glyph_I,
    ['J'] = (const uint8_t *)glyph_J,
    ['K'] = (const uint8_t *)glyph_K,
    ['L'] = (const uint8_t *)glyph_L,
    ['M'] = (const uint8_t *)glyph_M,
    ['N'] = (const uint8_t *)glyph_N,
    ['O'] = (const uint8_t *)glyph_O,
    ['P'] = (const uint8_t *)glyph_P,
    ['Q'] = (const uint8_t *)glyph_Q,
    ['R'] = (const uint8_t *)glyph_R,
    ['S'] = (const uint8_t *)glyph_S,
    ['T'] = (const uint8_t *)glyph_T,
    ['U'] = (const uint8_t *)glyph_U,
    ['V'] = (const uint8_t *)glyph_V,
    ['W'] = (const uint8_t *)glyph_W,
    ['X'] = (const uint8_t *)glyph_X,
    ['Y'] = (const uint8_t *)glyph_Y,
    ['Z'] = (const uint8_t *)glyph_Z,

    /* Lower */
    ['a'] = (const uint8_t *)glyph_a,
    ['b'] = (const uint8_t *)glyph_b,
    ['c'] = (const uint8_t *)glyph_c,
    ['d'] = (const uint8_t *)glyph_d,
    ['e'] = (const uint8_t *)glyph_e,
    ['f'] = (const uint8_t *)glyph_f,
    ['g'] = (const uint8_t *)glyph_g,
    ['h'] = (const uint8_t *)glyph_h,
    ['i'] = (const uint8_t *)glyph_i,
    ['j'] = (const uint8_t *)glyph_j,
    ['k'] = (const uint8_t *)glyph_k,
    ['l'] = (const uint8_t *)glyph_l,
    ['m'] = (const uint8_t *)glyph_m,
    ['n'] = (const uint8_t *)glyph_n,
    ['o'] = (const uint8_t *)glyph_o,
    ['p'] = (const uint8_t *)glyph_p,
    ['q'] = (const uint8_t *)glyph_q,
    ['r'] = (const uint8_t *)glyph_r,
    ['s'] = (const uint8_t *)glyph_s,
    ['t'] = (const uint8_t *)glyph_t,
    ['u'] = (const uint8_t *)glyph_u,
    ['v'] = (const uint8_t *)glyph_v,
    ['w'] = (const uint8_t *)glyph_w,
    ['x'] = (const uint8_t *)glyph_x,
    ['y'] = (const uint8_t *)glyph_y,
    ['z'] = (const uint8_t *)glyph_z,

    /* Numbers */
    ['0'] = (const uint8_t *)glyph_0,
    ['1'] = (const uint8_t *)glyph_1,
    ['2'] = (const uint8_t *)glyph_2,
    ['3'] = (const uint8_t *)glyph_3,
    ['4'] = (const uint8_t *)glyph_4,
    ['5'] = (const uint8_t *)glyph_5,
    ['6'] = (const uint8_t *)glyph_6,
    ['7'] = (const uint8_t *)glyph_7,
    ['8'] = (const uint8_t *)glyph_8,
    ['9'] = (const uint8_t *)glyph_9,

    /* Symbols */
    [' '] = (const uint8_t *)glyph_space,
    ['!'] = (const uint8_t *)glyph_exclamation_mark,
    ['?'] = (const uint8_t *)glyph_question_mark,
    ['$'] = (const uint8_t *)glyph_dollar,
    [':'] = (const uint8_t *)glyph_colon,
    ['.'] = (const uint8_t *)glyph_dot,
    ['_'] = (const uint8_t *)glyph_underscore,
    ['~'] = (const uint8_t *)glyph_tilde,
    ['@'] = (const uint8_t *)glyph_at,
    ['#'] = (const uint8_t *)glyph_hash,
    ['%'] = (const uint8_t *)glyph_percent,
    ['^'] = (const uint8_t *)glyph_caret,
    ['*'] = (const uint8_t *)glyph_asterisk,
    ['('] = (const uint8_t *)glyph_left_parenthesis,
    [')'] = (const uint8_t *)glyph_right_parenthesis,
    ['-'] = (const uint8_t *)glyph_minus,
    ['='] = (const uint8_t *)glyph_equals,
    ['+'] = (const uint8_t *)glyph_plus,
    ['['] = (const uint8_t *)glyph_left_square_bracket,
    [']'] = (const uint8_t *)glyph_right_square_bracket,
    ['{'] = (const uint8_t *)glyph_left_curly_brace,
    ['}'] = (const uint8_t *)glyph_right_curly_brace,
    [';'] = (const uint8_t *)glyph_semicolon,
    [','] = (const uint8_t *)glyph_comma,
    ['\''] = (const uint8_t *)glyph_single_quote,
    ['\"'] = (const uint8_t *)glyph_double_quote,
    ['<'] = (const uint8_t *)glyph_left_angle_bracket,
    ['>'] = (const uint8_t *)glyph_right_angle_bracket,
    ['/'] = (const uint8_t *)glyph_slash,
    ['\\'] = (const uint8_t *)glyph_backslash,
    ['|'] = (const uint8_t *)glyph_pipe,
    ['`'] = (const uint8_t *)glyph_backtick,
    ['&'] = (const uint8_t *)glyph_ampersand,
};

const uint8_t *font_get_glyph(char c);

void font_register_glyph(char c, const uint8_t *glyph);

void font_draw_char(char c, int x, int y, uint32_t color, int scale);

int font_draw_text(const char *s, int x, int y, uint32_t color, int scale, int spacing);

#endif /* FONT_H */
