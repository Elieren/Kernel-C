#include "../portio/portio.h"
#include "timer.h"
#include "../pic.h"
#include "clock/clock.h"
#include "../multitask/multitask.h"

/* PIT (Programmable Interval Timer) порты и команды */
#define PIT_CMD_PORT 0x43
#define PIT_COUNTER0 0x40
#define PIT_CMD_VALUE 0x36 // Бинарный счетчик, режим 3 (квадратная волна)

volatile uint16_t tick_time = 0;
volatile uint32_t seconds = 0;
volatile bool screen_refresh_status = true;

void timer_tick(void)
{
    tick_time++;
    if (tick_time >= 1000)
    {
        tick_time = 0;
        seconds++;
        clock_tick();
    }

    /* Экран обновляется каждые ~33 мс (при частоте ~30 Гц) */
    if ((tick_time % 33) == 0)
    {
        screen_refresh_status = true;
    }

    /* Отправляем EOI PIC перед возможным переключением контекста */
    pic_send_eoi(0);
}

void init_timer(uint32_t frequency)
{
    if (frequency == 0 || frequency > PIT_FREQUENCY)
    {
        return; /* Некорректная частота */
    }

    uint32_t divisor = PIT_FREQUENCY / frequency;

    /* Ограничиваем делитель для 16-разрядного счетчика */
    if (divisor > 0xFFFF)
    {
        divisor = 0xFFFF;
    }

    outb(PIT_CMD_PORT, PIT_CMD_VALUE);                    // Command port
    outb(PIT_COUNTER0, (uint8_t)(divisor & 0xFF));        // Low byte
    outb(PIT_COUNTER0, (uint8_t)((divisor >> 8) & 0xFF)); // High byte
}