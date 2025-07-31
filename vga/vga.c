#include "vga.h"
#include "../keyboard/portio.h"
#include "../time/timer.h"
#include "../time/clock/clock.h"

#define VGA_BUF ((uint8_t *)0xB8000)
#define CELL_SIZE 2

#define VGA_WIDTH 80
#define VGA_HEIGHT 25

#define VGA_CTRL 0x3D4
#define VGA_DATA 0x3D5
#define CURSOR_HIGH 0x0E
#define CURSOR_LOW 0x0F

extern uint32_t input_len;

extern uint32_t seconds;

uint8_t x;
uint8_t y;

const char *prompt = "$: ";

char time_str[9];
char time_up[9];

void clean_screen(void)
{
    uint8_t *vid = (uint8_t *)0xB8000;

    for (unsigned int i = 0; i < 80 * 25 * 2; i += 2)
    {
        vid[i] = ' ';      // сам символ
        vid[i + 1] = 0x07; // атрибут цвета
    }
}

uint8_t make_color(const uint8_t fore, const uint8_t back)
{
    return (back << 4) | (fore & 0x0F);
}

void print_char(const char c,
                const unsigned int x,
                const unsigned int y,
                const uint8_t fore,
                const uint8_t back)
{
    // проверка границ экрана
    if (x >= VGA_WIDTH || y >= VGA_HEIGHT)
        return;

    uint8_t *vid = (uint8_t *)0xB8000;
    uint8_t color = make_color(fore, back);

    // вычисляем смещение в байтах
    unsigned int offset = (y * VGA_WIDTH + x) * 2;

    vid[offset] = (uint8_t)c; // ASCII‑код символа
    vid[offset + 1] = color;  // атрибут цвета
}

// скроллим экран на одну строку
static void scroll_screen(void)
{
    uint8_t blank = make_color(0x07, 0x00);
    // сдвигаем строки: для каждой ячейки [row][col]
    for (int row = 1; row < VGA_HEIGHT; row++)
    {
        for (int col = 0; col < VGA_WIDTH; col++)
        {
            int dst = ((row - 1) * VGA_WIDTH + col) * CELL_SIZE;
            int src = (row * VGA_WIDTH + col) * CELL_SIZE;
            VGA_BUF[dst] = VGA_BUF[src];
            VGA_BUF[dst + 1] = VGA_BUF[src + 1];
        }
    }
    // очищаем последнюю строку
    int last_row = VGA_HEIGHT - 1;
    for (int col = 0; col < VGA_WIDTH; col++)
    {
        int off = (last_row * VGA_WIDTH + col) * CELL_SIZE;
        VGA_BUF[off] = ' ';
        VGA_BUF[off + 1] = blank;
    }
}

void print_char_long(const char c,
                     const uint8_t fore,
                     const uint8_t back)
{
    // горизонтальный перенос
    if (x >= VGA_WIDTH)
    {
        x = 0;
        y++;
    }
    // вертикальный перенос / скролл
    if (y >= VGA_HEIGHT)
    {
        scroll_screen();
        y = VGA_HEIGHT - 1;
    }

    int off = (y * VGA_WIDTH + x) * CELL_SIZE;
    VGA_BUF[off] = (uint8_t)c;
    VGA_BUF[off + 1] = make_color(fore, back);

    x++;

    update_hardware_cursor();
}

void new_line(void)
{
    x = 0;
    y += 1;

    for (uint32_t i = 0; prompt[i]; ++i)
    {
        print_char_long(prompt[i], WHITE, BLACK);
    }
}

void backspace(void)
{
    // Если пользователь не вводил ни одного символа после prompt — ничего не делаем
    if (input_len == 0)
    {
        return;
    }

    // Двигаем курсор назад
    if (x == 0)
    {
        // если в начале строки — перейти на предыдущую
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

    // Стираем символ в этой позиции
    uint8_t *vid = (uint8_t *)0xB8000;
    uint8_t color = make_color(WHITE, BLACK);
    unsigned int off = (y * VGA_WIDTH + x) * 2;
    vid[off] = ' ';
    vid[off + 1] = color;

    // Уменьшаем счётчик введённых символов
    input_len--;

    update_hardware_cursor();
}

void update_hardware_cursor(void)
{
    uint16_t pos = y * VGA_WIDTH + x;
    // старший байт
    outb(VGA_CTRL, CURSOR_HIGH);
    outb(VGA_DATA, (pos >> 8) & 0xFF);
    // младший байт
    outb(VGA_CTRL, CURSOR_LOW);
    outb(VGA_DATA, pos & 0xFF);
}

void format_up(unsigned total_seconds, char *buffer)
{
    unsigned hours = total_seconds / 3600;
    unsigned minutes = (total_seconds % 3600) / 60;
    unsigned seconds = total_seconds % 60;

    /* Число часов может быть больше 99, если нужно — увеличить разрядность. */
    buffer[0] = '0' + (hours / 10) % 10; /* старший разряд часов */
    buffer[1] = '0' + (hours % 10);      /* младший разряд часов */
    buffer[2] = ':';
    buffer[3] = '0' + (minutes / 10);
    buffer[4] = '0' + (minutes % 10);
    buffer[5] = ':';
    buffer[6] = '0' + (seconds / 10);
    buffer[7] = '0' + (seconds % 10);
    buffer[8] = '\0';
}

void print_systemup(void)
{
    format_up(seconds, time_up);

    for (uint32_t i = 0; time_up[i]; ++i)
    {
        print_char(time_up[i], 70 + i, 23, YELLOW, BLACK);
    }
}

void print_time(void)
{
    format_clock(time_str, system_clock);

    for (uint32_t i = 0; time_str[i]; ++i)
    {
        print_char(time_str[i], 70 + i, 24, YELLOW, BLACK);
    }
}