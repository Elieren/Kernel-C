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

typedef struct
{
    uint8_t day;   // день месяца: 1–31
    uint8_t month; // месяц:       1–12
    uint16_t year; // год, например 2026
} ClockDate;

extern volatile ClockTime system_clock;
extern volatile ClockDate system_date;

void clock_tick(); // вызывается из timer_handler
void clock_date_tick(void);
void format_clock(char *buffer, ClockTime t); // форматирует в "hh:mm:ss"
void format_date(char *buffer, ClockDate d);  // форматирует в "dd.mm.yyyy"
void init_system_clock(void);
void init_system_date(void);

#endif
