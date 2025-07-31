/* kernel.c */
#include "vga/vga.h"
#include "keyboard/keyboard.h"
#include "keyboard/portio.h"

#include "idt.h"
#include "time/timer.h"
#include "time/clock/clock.h"

#include <stdint.h>

uint32_t input_len = 0;

static inline uint32_t sys_print_char(
    uint32_t x,
    uint32_t y,
    char ch,
    uint8_t f_color,
    uint8_t b_color) // <-- новый параметр
{
    uint32_t ret;
    asm volatile(
        "int $0x80"
        : "=a"(ret)
        : "a"(0),       // номер syscall = 0 → EAX
          "b"(x),       // arg1 → EBX
          "c"(y),       // arg2 → ECX
          "d"(ch),      // arg3 → EDX
          "S"(f_color), // arg4 → ESI
          "D"(b_color)  // arg5 → EDI
        : "memory");
    return ret;
}

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
        print_char(prompt[i], i, 0, WHITE, BLACK);
    }

    sys_print_char(5, 10, 'X', WHITE, RED);

    for (;;)
    {
        asm volatile("hlt");
    }
}