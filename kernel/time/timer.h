#ifndef TIMER_H
#define TIMER_H

#include <stdint.h>
#include <stdbool.h>

#include <asm/timer.h>

extern volatile uint16_t tick_time;
extern volatile uint32_t seconds;
extern volatile bool screen_refresh_status;

void init_timer(uint32_t frequency);

/* Блокирующая задержка на указанное количество секунд */
void wait(uint32_t delay_seconds);
void mwait(uint32_t delay_milliseconds);

#endif