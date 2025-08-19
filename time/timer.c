#include "../portio/portio.h"
#include "../vga/vga.h"
#include "timer.h"
#include "../pic.h"
#include "clock/clock.h"
#include "../multitask/multitask.h"

volatile uint8_t tick_time = 0;
volatile uint32_t seconds = 0;

uint32_t *isr_timer_dispatch(uint32_t *regs_ptr)
{
    /* Это точка, где мы на каждом тике обрабатываем время и планировщик.
       Примечание: прерывания уже отключены в isr.asm (cli). */

    /* Обновляем экранное время раз в 100 тиксов (как у вас было) */
    tick_time++;
    if (tick_time >= 100)
    {
        update_hardware_cursor();
        tick_time = 0;
        seconds++;
        clock_tick();
    }

    /* Посылаем EOI PIC — делаем это здесь, до возможного переключения */
    pic_send_eoi(0);

    /* Запускаем scheduler: он сохранит regs текущей задачи и вернёт frame следующей. */
    uint32_t *out_regs = regs_ptr;
    schedule_from_isr(regs_ptr, &out_regs);

    return out_regs;
}

void init_timer(uint32_t frequency)
{
    uint32_t divisor = 1193180 / frequency;

    outb(0x43, 0x36);                  // Command port
    outb(0x40, divisor & 0xFF);        // Low byte
    outb(0x40, (divisor >> 8) & 0xFF); // High byte
}