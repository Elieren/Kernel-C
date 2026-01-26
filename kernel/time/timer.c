#include <asm/io.h>
#include "timer.h"
#include <asm/pic.h>
#include "clock/clock.h"
#include "kernel/sched/multitask/multitask.h"
#include <asm/cpu.h>

volatile uint16_t tick_time = 0;
volatile uint32_t seconds = 0;
volatile bool screen_refresh_status = true;

/* Блокирующая задержка на указанное количество секунд */
void wait(uint32_t delay_seconds)
{
    if (delay_seconds == 0)
        return;

    /* Запоминаем начальное время с максимальной точностью */
    uint32_t start_seconds = seconds;
    uint16_t start_tick_time = tick_time;
    uint16_t start_hw_count = timer_read_counter();

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

        /* Для еще большей точности на коротких интервалах используем аппаратный счетчик */
        if (elapsed_ticks + 10 >= target_ticks_total)
        {
            /* Когда осталось мало времени, переключаемся на точный подсчет */
            uint16_t current_hw_count = timer_read_counter();

            /* Получаем разницу в тиках аппаратного счетчика */
            uint32_t hw_ticks_passed = timer_get_ticks_delta(start_hw_count, current_hw_count);

            /* Конвертируем аппаратные тики в миллисекунды */
            uint32_t additional_ms = timer_ticks_to_ms(hw_ticks_passed);
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
    uint16_t start_hw_count = timer_read_counter();

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

        /* Для еще большей точности на коротких интервалах используем аппаратный счетчик */
        if (elapsed_milliseconds + 10 >= target_milliseconds)
        {
            /* Когда осталось мало времени, переключаемся на точный подсчет */
            uint16_t current_hw_count = timer_read_counter();

            /* Получаем разницу в тиках аппаратного счетчика */
            uint32_t hw_ticks_passed = timer_get_ticks_delta(start_hw_count, current_hw_count);

            /* Конвертируем в миллисекунды */
            uint32_t additional_ms = timer_ticks_to_ms(hw_ticks_passed);
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
    static uint32_t accumulated_error = 0;

    tick_time++;

    accumulated_error += 180;
    if (accumulated_error >= 1000)
    {
        tick_time++;
        accumulated_error -= 1000;
    }

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
    timer_init(frequency);
}