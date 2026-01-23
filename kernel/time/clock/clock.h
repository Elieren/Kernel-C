// clock.h

#ifndef CLOCK_H
#define CLOCK_H

#include <stdint.h>

typedef struct
{
    uint8_t hh; // часы:   0–23
    uint8_t mm; // минуты: 0–59
    uint8_t ss; // секунды:0–59
} ClockTime;

extern volatile ClockTime system_clock;

void clock_tick();                            // вызывается из timer_handler
void format_clock(char *buffer, ClockTime t); // форматирует в "hh:mm:ss"
void init_system_clock(void);

#endif
