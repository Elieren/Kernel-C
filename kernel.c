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

    // clean_screen();

    // sys_print_str(msg, 78, 11, WHITE, BLACK);

    clean_screen();

    update_hardware_cursor();

    for (uint32_t i = 0; prompt[i]; ++i)
    {
        print_char_long(prompt[i], WHITE, BLACK);
    }

    sys_print_char('X', 5, 10, WHITE, RED);

    const char *secs = sys_get_seconds_str();

    sys_print_str(secs, 0, 20, WHITE, RED);

    for (;;)
    {
        asm volatile("hlt");
    }
}