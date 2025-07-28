#include "../keyboard/portio.h"
#include "../vga/vga.h"
#include "timer.h"
#include "../pic.h"

volatile uint32_t tick_time = 0;

void timer_handler()
{
    tick_time++;
    if ((tick_time % (100 * 3)) == 0)
    {
        clean_screen();
        update_hardware_cursor();
    }
    // говорим PIC: «я обработал IRQ0»
    pic_send_eoi(0);
}

void init_timer(uint32_t frequency)
{
    uint32_t divisor = 1193180 / frequency;

    outb(0x43, 0x36);                  // Command port
    outb(0x40, divisor & 0xFF);        // Low byte
    outb(0x40, (divisor >> 8) & 0xFF); // High byte
}