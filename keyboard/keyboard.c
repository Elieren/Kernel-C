#include "keyboard.h"
#include "../vga/vga.h"
#include "portio.h"
#include "../pic.h"

extern uint32_t input_len;

static bool shift_down = false;
static bool caps_lock = false;

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
    [KEY_APOSTROPHE] = '\'',
    [KEY_GRAVE] = '`',

    [KEY_SPACE] = ' ',
    [KEY_ENTER] = '\n',
    [KEY_TAB] = '\t',
    [KEY_BACKSPACE] = '\b',
};

static const char scancode_to_ascii_shifted[256] = {
    [KEY_A] = 'A',
    [KEY_B] = 'B',
    [KEY_C] = 'C',
    [KEY_D] = 'D',
    [KEY_E] = 'E',
    [KEY_F] = 'F',
    [KEY_G] = 'G',
    [KEY_H] = 'H',
    [KEY_I] = 'I',
    [KEY_J] = 'J',
    [KEY_K] = 'K',
    [KEY_L] = 'L',
    [KEY_M] = 'M',
    [KEY_N] = 'N',
    [KEY_O] = 'O',
    [KEY_P] = 'P',
    [KEY_Q] = 'Q',
    [KEY_R] = 'R',
    [KEY_S] = 'S',
    [KEY_T] = 'T',
    [KEY_U] = 'U',
    [KEY_V] = 'V',
    [KEY_W] = 'W',
    [KEY_X] = 'X',
    [KEY_Y] = 'Y',
    [KEY_Z] = 'Z',

    [KEY_1] = '!',
    [KEY_2] = '@',
    [KEY_3] = '#',
    [KEY_4] = '$',
    [KEY_5] = '%',
    [KEY_6] = '^',
    [KEY_7] = '&',
    [KEY_8] = '*',
    [KEY_9] = '(',
    [KEY_0] = ')',

    [KEY_MINUS] = '_',
    [KEY_EQUAL] = '+',
    [KEY_SQUARE_OPEN_BRACKET] = '{',
    [KEY_SQUARE_CLOSE_BRACKET] = '}',
    [KEY_SEMICOLON] = ':',
    [KEY_BACKSLASH] = '|',
    [KEY_COMMA] = '<',
    [KEY_DOT] = '>',
    [KEY_FORESLHASH] = '?',
    [KEY_APOSTROPHE] = '\"',
    [KEY_GRAVE] = '~',

    [KEY_SPACE] = ' ',
    [KEY_ENTER] = '\n',
    [KEY_TAB] = '\t',
    [KEY_BACKSPACE] = '\b',
};

char my_toupper(char c)
{
    if (c >= 'a' && c <= 'z')
    {
        return c - 32;
    }
    return c;
}

bool is_alpha(uint8_t scancode)
{
    switch (scancode)
    {
    case KEY_A:
    case KEY_B:
    case KEY_C:
    case KEY_D:
    case KEY_E:
    case KEY_F:
    case KEY_G:
    case KEY_H:
    case KEY_I:
    case KEY_J:
    case KEY_K:
    case KEY_L:
    case KEY_M:
    case KEY_N:
    case KEY_O:
    case KEY_P:
    case KEY_Q:
    case KEY_R:
    case KEY_S:
    case KEY_T:
    case KEY_U:
    case KEY_V:
    case KEY_W:
    case KEY_X:
    case KEY_Y:
    case KEY_Z:
        return true;
    default:
        return false;
    }
}

// Преобразование сканкода в ASCII (или 0, если нет соответствия)
char get_ascii_char(uint8_t scancode)
{
    if (is_alpha(scancode))
    {
        bool upper = shift_down ^ caps_lock;
        char base = scancode_to_ascii[(uint8_t)scancode]; // 'a'–'z'
        return upper ? my_toupper(base) : base;
    }

    if (shift_down)
    {
        return scancode_to_ascii_shifted[(uint8_t)scancode];
    }
    else
    {
        return scancode_to_ascii[(uint8_t)scancode];
    }
}

void keyboard_handler(void)
{
    uint8_t code = inb(KEYBOARD_PORT);

    // Проверяем Break‑код (высокий бит = 1)
    bool released = code & 0x80;
    uint8_t key = code & 0x7F;

    if (key == KEY_LSHIFT || key == KEY_RSHIFT)
    {
        shift_down = !released;
        pic_send_eoi(1);
        return;
    }
    if (key == KEY_CAPSLOCK && !released)
    {
        // при нажатии (make-код) — переключаем
        caps_lock = !caps_lock;
        pic_send_eoi(1);
        return;
    }

    // Обработка Make‑кодов остальных клавиш
    if (!released)
    {
        char ch = get_ascii_char(key);
        if (ch)
        {
            if (key == KEY_ENTER)
            {
                new_line();
                input_len = 0;
            }
            else if (key == KEY_BACKSPACE)
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

    pic_send_eoi(1);
}