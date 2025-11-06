#include "../portio/portio.h"
#include "timer.h"
#include "../pic.h"
#include "clock/clock.h"
#include "../multitask/multitask.h"
#include "../vga/graphics.h"

volatile uint16_t tick_time = 0;
volatile uint32_t seconds = 0;

void timer_tick(void)
{
    tick_time++;
    if (tick_time >= 1000)
    {
        tick_time = 0;
        seconds++;
        clock_tick();
    }

    if ((tick_time % 33) == 0)
    {
        gfx_clear(0x00000000);
        gfx_draw_all_from_cells();
    }

    /* Посылаем EOI PIC — делаем это здесь, до возможного переключения */
    pic_send_eoi(0);
}

void init_timer(uint32_t frequency)
{
    uint32_t divisor = 1193180 / frequency;

    outb(0x43, 0x36);                  // Command port
    outb(0x40, divisor & 0xFF);        // Low byte
    outb(0x40, (divisor >> 8) & 0xFF); // High byte
}