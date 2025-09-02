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

#define SYSCALL_PRINT_CHAR 0
#define SYSCALL_PRINT_STRING 1
#define SYSCALL_GETCHAR 30
#define SYSCALL_SETPOSCURSOR 31

static char screen_chars[VGA_HEIGHT][VGA_WIDTH];
static uint8_t screen_attr[VGA_HEIGHT][VGA_WIDTH];

/* Прототипы функций */
void new_line(void);
static void scroll_screen_syscall(void);
void print_char_to_buffer(char c, int col, int row, uint8_t fore, uint8_t back);
void print_str_to_buffer(const char *str, int col, int row, uint8_t fore, uint8_t back);
void backspace(void);

static inline void sys_print_str(const char *s, uint32_t x, uint32_t y, uint8_t fg, uint8_t bg);
static inline void sys_print_char(char ch, uint32_t x, uint32_t y, uint8_t fg, uint8_t bg);
static inline const char sys_getchar(void);
static inline void sys_setposcursor(uint32_t x, uint32_t y);

/* -------------------------------------------------
   _start в начале файла
------------------------------------------------- */
void main(void)
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
        if (ch == '\0' || ch == ' ')
        {
            asm volatile("hlt");
            continue;
        } // пустой буфер

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

/* -------------------------------------------------
   Определения остальных функций
------------------------------------------------- */
static inline void sys_print_str(const char *s, uint32_t x, uint32_t y, uint8_t fg, uint8_t bg)
{
    register uint64_t rax_reg asm("rax") = (uint64_t)SYSCALL_PRINT_STRING;
    register uint64_t rdi_reg asm("rdi") = (uint64_t)s;
    register uint64_t rsi_reg asm("rsi") = (uint64_t)x;
    register uint64_t rdx_reg asm("rdx") = (uint64_t)y;
    register uint64_t r10_reg asm("r10") = (uint64_t)fg;
    register uint64_t r8_reg asm("r8") = (uint64_t)bg;

    asm volatile("int $0x80"
                 : "+r"(rax_reg)
                 : "r"(rdi_reg), "r"(rsi_reg), "r"(rdx_reg),
                   "r"(r10_reg), "r"(r8_reg)
                 : "rcx", "r11", "memory");
}

static inline void sys_print_char(char ch, uint32_t x, uint32_t y, uint8_t fg, uint8_t bg)
{
    register uint64_t rax_reg asm("rax") = (uint64_t)SYSCALL_PRINT_CHAR;
    register uint64_t rdi_reg asm("rdi") = (uint64_t)(uint8_t)ch;
    register uint64_t rsi_reg asm("rsi") = (uint64_t)x;
    register uint64_t rdx_reg asm("rdx") = (uint64_t)y;
    register uint64_t r10_reg asm("r10") = (uint64_t)fg;
    register uint64_t r8_reg asm("r8") = (uint64_t)bg;

    asm volatile("int $0x80"
                 : "+r"(rax_reg)
                 : "r"(rdi_reg), "r"(rsi_reg), "r"(rdx_reg),
                   "r"(r10_reg), "r"(r8_reg)
                 : "rcx", "r11", "memory");
}

static inline const char sys_getchar(void)
{
    register uint64_t rax_reg asm("rax") = (uint64_t)SYSCALL_GETCHAR;

    asm volatile("int $0x80"
                 : "+r"(rax_reg)
                 :
                 : "rcx", "r11", "memory");

    return (char)(rax_reg & 0xFF);
}

static inline void sys_setposcursor(uint32_t x, uint32_t y)
{
    register uint64_t rax_reg asm("rax") = (uint64_t)SYSCALL_SETPOSCURSOR;
    register uint64_t rdi_reg asm("rdi") = (uint64_t)x;
    register uint64_t rsi_reg asm("rsi") = (uint64_t)y;

    asm volatile("int $0x80"
                 : "+r"(rax_reg)
                 : "r"(rdi_reg), "r"(rsi_reg)
                 : "rcx", "r11", "memory");
}

void new_line(void)
{
    x = 0;
    y += 1;
    sys_setposcursor(x, y);
}

static void scroll_screen_syscall(void)
{
    for (int row = 1; row < VGA_HEIGHT; row++)
    {
        for (int col = 0; col < VGA_WIDTH; col++)
        {
            screen_chars[row - 1][col] = screen_chars[row][col];
            screen_attr[row - 1][col] = screen_attr[row][col];
        }
    }

    for (int col = 0; col < VGA_WIDTH; col++)
    {
        screen_chars[VGA_HEIGHT - 1][col] = ' ';
        screen_attr[VGA_HEIGHT - 1][col] = (BLACK << 4) | GREY;
    }

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

void print_char_to_buffer(char c, int col, int row, uint8_t fore, uint8_t back)
{
    screen_chars[row][col] = c;
    screen_attr[row][col] = (back << 4) | (fore & 0x0F);
    sys_print_char(c, col, row, fore, back);
}

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

    sys_print_str(str, col, row, fore, back);
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
