#include <asm/timer.h>
#include <asm/io.h>

/* Чтение текущего значения счетчика PIT */
uint16_t timer_read_counter(void)
{
    uint16_t count = 0;

    /* Команда latch для канала 0 */
    io_write8(PIT_CMD_PORT, 0x00);

    /* Читаем младший и старший байты */
    count = io_read8(PIT_COUNTER0);
    count |= io_read8(PIT_COUNTER0) << 8;

    return count;
}

/* Вычисление разницы между двумя значениями счетчика PIT */
uint32_t timer_get_ticks_delta(uint16_t start_count, uint16_t current_count)
{
    /* PIT считает в обратном направлении от максимального значения до 0 */
    uint32_t ticks_passed;

    if (current_count <= start_count)
    {
        ticks_passed = start_count - current_count;
    }
    else
    {
        /* Произошло переполнение PIT */
        ticks_passed = (0xFFFF - current_count) + start_count;
    }

    return ticks_passed;
}

/* Конвертация тиков PIT в миллисекунды */
uint32_t timer_ticks_to_ms(uint32_t ticks)
{
    uint64_t ms_precise = ((uint64_t)ticks * 1000ULL + (PIT_FREQUENCY / 2)) / PIT_FREQUENCY;
    return (uint32_t)ms_precise;
}

/* Инициализация PIT таймера */
void timer_init(uint32_t frequency)
{
    if (frequency == 0 || frequency > PIT_FREQUENCY)
    {
        return; /* Некорректная частота */
    }

    uint64_t divisor_precise = ((uint64_t)PIT_FREQUENCY * 1000ULL + (frequency / 2)) / frequency;
    divisor_precise = (divisor_precise + 500) / 1000;

    uint32_t divisor = (uint32_t)divisor_precise;

    if (divisor > 0xFFFF)
    {
        divisor = 0xFFFF;
    }
    if (divisor < 1)
    {
        divisor = 1;
    }

    io_write8(PIT_CMD_PORT, PIT_CMD_VALUE);                    /* Command port */
    io_write8(PIT_COUNTER0, (uint8_t)(divisor & 0xFF));        /* Low byte */
    io_write8(PIT_COUNTER0, (uint8_t)((divisor >> 8) & 0xFF)); /* High byte */
}