#include "../syscall/syscall.h"
#include <stdint.h>
#include <stddef.h>

#define VGA_WIDTH 80
#define VGA_HEIGHT 25
#define INTERNAL_SPACE 0x01

uint8_t x;
uint8_t y;

uint32_t input_len;

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

// Внутренний буфер экрана
static char screen_chars[VGA_HEIGHT][VGA_WIDTH];
static uint8_t screen_attr[VGA_HEIGHT][VGA_WIDTH];

void new_line(void)
{
    x = 0;
    y += 1;

    sys_setposcursor(x, y);
}

// Скролл с использованием буфера
static void scroll_screen_syscall(void)
{
    // Сдвигаем строки вверх
    for (int row = 1; row < VGA_HEIGHT; row++)
    {
        for (int col = 0; col < VGA_WIDTH; col++)
        {
            screen_chars[row - 1][col] = screen_chars[row][col];
            screen_attr[row - 1][col] = screen_attr[row][col];
        }
    }

    // Очищаем последнюю строку в буфере
    for (int col = 0; col < VGA_WIDTH; col++)
    {
        screen_chars[VGA_HEIGHT - 1][col] = ' ';
        screen_attr[VGA_HEIGHT - 1][col] = (BLACK << 4) | GREY;
    }

    // Перерисовываем весь экран из буфера
    for (int row = 0; row < VGA_HEIGHT; row++)
    {
        for (int col = 0; col < VGA_WIDTH; col++)
        {
            char c = screen_chars[row][col];
            uint8_t attr = screen_attr[row][col];
            uint8_t fore = attr & 0x0F;
            uint8_t back = attr >> 4;
            sys_print_char(c, col, row, fore, back);
        }
    }
}

// Печать символа с обновлением внутреннего буфера
void print_char_to_buffer(char c, int col, int row, uint8_t fore, uint8_t back)
{
    screen_chars[row][col] = c;
    screen_attr[row][col] = (back << 4) | (fore & 0x0F);
    sys_print_char(c, col, row, fore, back);
}

// Печать строки в буфер и на экран
void print_str_to_buffer(const char *str, int col, int row, uint8_t fore, uint8_t back)
{
    int c = col;
    int r = row;

    for (const char *p = str; *p; ++p)
    {
        screen_chars[r][c] = *p;
        screen_attr[r][c] = (back << 4) | (fore & 0x0F);
        c++;
    }

    // Вывод всей строки на экран
    sys_print_str(str, col, row, fore, back);

    // Обновляем глобальные координаты курсора
    sys_setposcursor(x, y);
}

void backspace(void)
{
    if (input_len == 0)
        return;

    if (x == 0)
    {
        if (y > 0)
        {
            y--;
            x = VGA_WIDTH - 1;
        }
    }
    else
    {
        x--;
    }

    print_char_to_buffer(' ', x, y, WHITE, BLACK);
    input_len--;
    sys_setposcursor(x, y);
}

void terminal_entry(void)
{
    x = 0;
    y = 0;
    input_len = 0;

    // Приветствие
    for (int col = 0; col < VGA_WIDTH; col++)
    {
        screen_chars[0][col] = ' ';
        screen_attr[0][col] = (BLACK << 4) | WHITE;
    }
    print_str_to_buffer("SimpleTerm v0.1", x, y, WHITE, BLACK);
    y++;

    // Показ prompt
    print_str_to_buffer("$: ", x, y, WHITE, BLACK);
    x = 3;
    sys_setposcursor(x, y);

    for (;;)
    {
        char ch = sys_getchar();
        if (ch == '\0' || ch == ' ') // пустой буфер
            continue;

        if (ch == '\n')
        {
            new_line();
            input_len = 0;

            if (y >= VGA_HEIGHT)
            {
                scroll_screen_syscall();
                y = VGA_HEIGHT - 1;
            }

            print_str_to_buffer("$: ", 0, y, WHITE, BLACK);
            x = 3;

            sys_setposcursor(x, y);
        }
        else if (ch == '\b')
        {
            if (input_len > 0)
                backspace();
        }
        else
        {
            if (ch == INTERNAL_SPACE)
                ch = ' ';

            print_char_to_buffer(ch, x, y, WHITE, BLACK);
            x++;
            input_len++;

            if (x >= VGA_WIDTH)
            {
                x = 0;
                y++;
                if (y >= VGA_HEIGHT)
                {
                    scroll_screen_syscall();
                    y = VGA_HEIGHT - 1;
                }
            }

            sys_setposcursor(x, y);
        }
    }
}
