#include "keyboard.h"
#include "../vga/vga.h"
#include "portio.h"

extern uint32_t input_len;

char get_input_keycode(void)
{
    uint8 code;
    // ждем make‑код
    do
    {
        // статусный порт 0x64, бит0=1 когда готово
        while (!(inb(0x64) & 1))
        { /* nop */
        }
        code = inb(KEYBOARD_PORT);
    } while (code & 0x80); // пропускаем все коды с установленным старшим битом
    return (char)code;
}

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
char get_ascii_char(uint8 scancode)
{
    return scancode_to_ascii[(uint8)scancode];
}

/*
keep the cpu busy for doing nothing(nop)
so that io port will not be processed by cpu
here timer can also be used, but lets do this in looping counter
*/
void wait_for_io(uint32 timer_count)
{
    while (1)
    {
        asm volatile("nop");
        timer_count--;
        if (timer_count <= 0)
            break;
    }
}

void sleep(uint32 timer_count)
{
    wait_for_io(timer_count);
}

void input(void)
{
    char ch = 0;
    char keycode = 0;

    while (1)
    {
        keycode = get_input_keycode();
        if (keycode == KEY_ENTER)
        {
            new_line();
            input_len = 0;
        }
        else if (keycode == KEY_BACKSPACE)
        {
            if (input_len > 0)
            {
                backspace();
            }
        }
        else
        {
            ch = get_ascii_char(keycode);
            print_char_long(ch, WHITE, BLACK);
            input_len++;
        }
        // sleep(0x02FFFFFF);
    }
}