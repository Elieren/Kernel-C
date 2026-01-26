#ifndef X86_64_TIMER_H
#define X86_64_TIMER_H

#include <stdint.h>

/* PIT - базовая частота */
#define PIT_FREQUENCY 1193180U

/* PIT порты и команды */
#define PIT_CMD_PORT 0x43
#define PIT_COUNTER0 0x40
#define PIT_CMD_VALUE 0x36 /* Бинарный счетчик, режим 3 (квадратная волна) */

/* Чтение текущего значения аппаратного счетчика */
uint16_t timer_read_counter(void);

/* Вычисление разницы между двумя значениями счетчика (с учетом переполнения) */
uint32_t timer_get_ticks_delta(uint16_t start_count, uint16_t current_count);

/* Конвертация аппаратных тиков в миллисекунды */
uint32_t timer_ticks_to_ms(uint32_t ticks);

/* Инициализация аппаратного таймера */
void timer_init(uint32_t frequency);

#endif