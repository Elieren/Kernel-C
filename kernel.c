/* kernel.c */
#include "vga/vga.h"
#include "keyboard/keyboard.h"
#include "keyboard/portio.h"

#include "idt.h"
#include "time/timer.h"

#include <stdint.h>

uint32_t input_len = 0;

void kmain(void)
{
    idt_install();
    init_timer(1);
    outb(0x21, 0xFC);

    __asm__ volatile("sti");

    const char *msg = "Hello, World!";
    uint8_t *vid = (uint8_t *)0xB8000;

    clean_screen();

    for (uint32_t i = 0; msg[i]; ++i)
    {
        print_char(msg[i], i, 0, YELLOW, BLACK);
    }

    clean_screen();

    update_hardware_cursor();

    const char *prompt = "$: ";

    for (uint32_t i = 0; prompt[i]; ++i)
    {
        print_char(prompt[i], i, 0, WHITE, BLACK);
    }

    for (;;)
        ;
}