/* kernel.c */
#include "vga/vga.h"

typedef unsigned char uint8_t;
typedef unsigned int uint32_t;

void clean_screen(void);

void kmain(void)
{
    const char *msg = "Hello, World!";
    uint8_t *vid = (uint8_t *)0xB8000;

    clean_screen();

    for (uint32_t i = 0; msg[i]; ++i)
    {
        print_char(msg[i], i, 0, YELLOW, BLACK);
    }
    for (;;)
        ; // бесконечный цикл
}