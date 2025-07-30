#include "keyboard.h"
#include "../vga/vga.h"
#include "portio.h"
#include "../pic.h"

extern uint32_t input_len;

// Таблица 0–255, все неиспользуемые элементы = 0
static const char scancode_to_ascii[256] = {
    [KEY_A] = 'a',
    [KEY_B] = 'b',
    [KEY_C] = 'c',
    [KEY_D] = 'd',
    [KEY_E] = 'e',
    [KEY_F] = 'f',
    [KEY_G] = 'g',
    [KEY_H] = 'h',
    [KEY_I] = 'i',
    [KEY_J] = 'j',
    [KEY_K] = 'k',
    [KEY_L] = 'l',
    [KEY_M] = 'm',
    [KEY_N] = 'n',
    [KEY_O] = 'o',
    [KEY_P] = 'p',
    [KEY_Q] = 'q',
    [KEY_R] = 'r',
    [KEY_S] = 's',
    [KEY_T] = 't',
    [KEY_U] = 'u',
    [KEY_V] = 'v',
    [KEY_W] = 'w',
    [KEY_X] = 'x',
    [KEY_Y] = 'y',
    [KEY_Z] = 'z',

    [KEY_1] = '1',
    [KEY_2] = '2',
    [KEY_3] = '3',
    [KEY_4] = '4',
    [KEY_5] = '5',
    [KEY_6] = '6',
    [KEY_7] = '7',
    [KEY_8] = '8',
    [KEY_9] = '9',
    [KEY_0] = '0',

    [KEY_MINUS] = '-',
    [KEY_EQUAL] = '=',
    [KEY_SQUARE_OPEN_BRACKET] = '[',
    [KEY_SQUARE_CLOSE_BRACKET] = ']',
    [KEY_SEMICOLON] = ';',
    [KEY_BACKSLASH] = '\\',
    [KEY_COMMA] = ',',
    [KEY_DOT] = '.',
    [KEY_FORESLHASH] = '/',

    [KEY_SPACE] = ' ',
    [KEY_ENTER] = '\n',
    [KEY_TAB] = '\t',
    [KEY_BACKSPACE] = '\b',
};

// Преобразование сканкода в ASCII (или 0, если нет соответствия)
char get_ascii_char(uint8_t scancode)
{
    return scancode_to_ascii[(uint8_t)scancode];
}

void keyboard_handler(void)
{
    uint8_t code = inb(KEYBOARD_PORT);
    if (!(code & 0x80))
    { // make‑code только
        char ch = get_ascii_char(code);
        if (ch)
        {
            if (code == KEY_ENTER)
            {
                new_line();
                input_len = 0;
            }
            else if (code == KEY_BACKSPACE)
            {
                if (input_len > 0)
                {
                    backspace();
                }
            }
            else
            {
                print_char_long(ch, WHITE, BLACK);
                input_len++;
            }
        }
    }
    pic_send_eoi(1); // EOI для клавиатуры
}