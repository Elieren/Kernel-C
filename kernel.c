/* kernel.c */
#include "vga/vga.h"
#include "keyboard/keyboard.h"
#include "keyboard/portio.h"

#include "idt.h"
#include "time/timer.h"
#include "time/clock/clock.h"
#include "syscall/syscall.h"

#include <stdint.h>

uint32_t input_len = 0;

void kmain(void)
{
    idt_install();
    init_system_clock();
    init_timer(1);
    outb(0x21, 0xFC);

    asm volatile("sti");

    // const char *msg = "Hello, World!";
    // uint8_t *vid = (uint8_t *)0xB8000;

    // clean_screen();

    // for (uint32_t i = 0; msg[i]; ++i)
    // {
    //     print_char(msg[i], i, 0, YELLOW, BLACK);
    // }

    clean_screen();

    update_hardware_cursor();

    for (uint32_t i = 0; prompt[i]; ++i)
    {
        print_char_long(prompt[i], WHITE, BLACK);
    }

    sys_print_char(5, 10, 'X', WHITE, RED);

    const char *secs = sys_get_seconds_str();

    for (uint32_t i = 0; secs[i]; ++i)
    {
        sys_print_char(i, 20, secs[i], WHITE, RED);
    }

    for (;;)
    {
        asm volatile("hlt");
    }
}