#include <asm/io.h>
#include "timer.h"
#include <asm/pic.h>
#include "clock/clock.h"
#include "kernel/sched/multitask/multitask.h"
#include <asm/cpu.h>

/* PIT (Programmable Interval Timer) порты и команды */
#define PIT_CMD_PORT 0x43
#define PIT_COUNTER0 0x40
#define PIT_CMD_VALUE 0x36 // Бинарный счетчик, режим 3 (квадратная волна)

volatile uint16_t tick_time = 0;
volatile uint32_t seconds = 0;
volatile bool screen_refresh_status = true;

static uint16_t read_pit_count(void)
{
    uint16_t count = 0;

    /* Команда latch для канала 0 */
    io_write8(PIT_CMD_PORT, 0x00);

    /* Читаем младший и старший байты */
    count = io_read8(PIT_COUNTER0);
    count |= io_read8(PIT_COUNTER0) << 8;

    return count;
}

/* Абсолютно точная функция wait с busy-wait */
void wait(uint32_t delay_seconds)
{
    if (delay_seconds == 0)
        return;

    /* Запоминаем начальное время с максимальной точностью */
    uint32_t start_seconds = seconds;
    uint16_t start_tick_time = tick_time;
    uint16_t start_pit_count = read_pit_count();

    /* Рассчитываем целевое количество тиков */
    /* При частоте таймера 1000 Гц: 1 секунда = 1000 тиков */
    uint32_t target_ticks_total = delay_seconds * 1000;
    uint32_t elapsed_ticks = 0;

    /* Точный busy-wait цикл */
    while (elapsed_ticks < target_ticks_total)
    {
        /* Вычисляем прошедшие секунды */
        uint32_t current_seconds = seconds;
        uint16_t current_tick_time = tick_time;

        /* Вычисляем прошедшее время в тиках */
        if (current_seconds == start_seconds)
        {
            /* Все еще в той же секунде */
            if (current_tick_time >= start_tick_time)
            {
                elapsed_ticks = current_tick_time - start_tick_time;
            }
            else
            {
                /* Переполнение tick_time в пределах той же секунды */
                elapsed_ticks = (1000 - start_tick_time) + current_tick_time;
            }
        }
        else
        {
            /* Прошла как минимум одна полная секунда */
            uint32_t seconds_passed = current_seconds - start_seconds;

            if (current_tick_time >= start_tick_time)
            {
                elapsed_ticks = seconds_passed * 1000 +
                                (current_tick_time - start_tick_time);
            }
            else
            {
                elapsed_ticks = seconds_passed * 1000 -
                                (start_tick_time - current_tick_time);
            }
        }

        /* Для еще большей точности на коротких интервалах используем PIT */
        if (elapsed_ticks + 10 >= target_ticks_total)
        {
            /* Когда осталось мало времени, переключаемся на точный подсчет */
            uint16_t current_pit_count = read_pit_count();

            /* PIT считает в обратном направлении от 0xFFFF до 0 */
            uint32_t pit_ticks_passed;
            if (current_pit_count <= start_pit_count)
            {
                pit_ticks_passed = start_pit_count - current_pit_count;
            }
            else
            {
                /* Произошло переполнение PIT */
                pit_ticks_passed = (0xFFFF - start_pit_count) + current_pit_count;
            }

            /* Пересчитываем PIT тики в миллисекунды (зависит от частоты таймера) */
            uint32_t additional_ms = pit_ticks_passed / 1193; /* 1193180 / 1000 ≈ 1193 */
            uint32_t total_elapsed = elapsed_ticks + additional_ms;

            if (total_elapsed >= target_ticks_total)
            {
                break;
            }
        }

        /* Небольшая пауза в цикле, чтобы не нагружать процессор на 100% */
        cpu_relax();
    }
}

void mwait(uint32_t delay_milliseconds)
{
    if (delay_milliseconds == 0)
        return;

    /* Запоминаем начальное время с максимальной точностью */
    uint32_t start_seconds = seconds;
    uint16_t start_tick_time = tick_time;
    uint16_t start_pit_count = read_pit_count();

    /* Рассчитываем целевое количество миллисекунд */
    /* При частоте таймера 1000 Гц: 1 мс = 1 тик */
    uint32_t target_milliseconds = delay_milliseconds;
    uint32_t elapsed_milliseconds = 0;

    /* Точный busy-wait цикл */
    while (elapsed_milliseconds < target_milliseconds)
    {
        /* Вычисляем текущее время */
        uint32_t current_seconds = seconds;
        uint16_t current_tick_time = tick_time;

        /* Вычисляем прошедшее время в миллисекундах */
        if (current_seconds == start_seconds)
        {
            /* Все еще в той же секунде */
            if (current_tick_time >= start_tick_time)
            {
                elapsed_milliseconds = current_tick_time - start_tick_time;
            }
            else
            {
                /* Переполнение tick_time в пределах той же секунды */
                /* Это не должно происходить при delay_milliseconds < 1000 */
                elapsed_milliseconds = (1000 - start_tick_time) + current_tick_time;
            }
        }
        else
        {
            /* Прошла как минимум одна полная секунда */
            uint32_t seconds_passed = current_seconds - start_seconds;

            if (current_tick_time >= start_tick_time)
            {
                elapsed_milliseconds = seconds_passed * 1000 +
                                       (current_tick_time - start_tick_time);
            }
            else
            {
                elapsed_milliseconds = seconds_passed * 1000 -
                                       (start_tick_time - current_tick_time);
            }
        }

        /* Для еще большей точности на коротких интервалах используем PIT */
        if (elapsed_milliseconds + 10 >= target_milliseconds)
        {
            /* Когда осталось мало времени, переключаемся на точный подсчет */
            uint16_t current_pit_count = read_pit_count();

            /* PIT считает в обратном направлении от 0xFFFF до 0 */
            uint32_t pit_ticks_passed;
            if (current_pit_count <= start_pit_count)
            {
                pit_ticks_passed = start_pit_count - current_pit_count;
            }
            else
            {
                /* Произошло переполнение PIT */
                pit_ticks_passed = (0xFFFF - start_pit_count) + current_pit_count;
            }

            /* Пересчитываем PIT тики в миллисекунды */
            /* 1193180 Гц / 1000 = 1193.18 тиков на миллисекунду */
            uint32_t additional_ms = pit_ticks_passed / 1193;
            uint32_t total_elapsed = elapsed_milliseconds + additional_ms;

            if (total_elapsed >= target_milliseconds)
            {
                break;
            }
        }

        /* Небольшая пауза в цикле */
        cpu_relax();
    }
}

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

    io_write8(PIT_CMD_PORT, PIT_CMD_VALUE);                    // Command port
    io_write8(PIT_COUNTER0, (uint8_t)(divisor & 0xFF));        // Low byte
    io_write8(PIT_COUNTER0, (uint8_t)((divisor >> 8) & 0xFF)); // High byte
}