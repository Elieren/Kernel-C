// clock.c

#include "clock.h"
#include "rtc.h"

uint32_t h, m, s;
volatile ClockTime system_clock = {0, 0, 0}; // старт с 00:00:00

void init_system_clock(void)
{
    read_rtc_time(&h, &m, &s);
    system_clock.hh = (uint8_t)h;
    system_clock.mm = (uint8_t)m;
    system_clock.ss = (uint8_t)s;
}

void clock_tick()
{
    system_clock.ss++;

    if (system_clock.ss >= 60)
    {
        system_clock.ss = 0;
        system_clock.mm++;

        if (system_clock.mm >= 60)
        {
            system_clock.mm = 0;
            system_clock.hh++;

            if (system_clock.hh >= 24)
            {
                system_clock.hh = 0;
            }
        }
    }
}

// Форматирует структуру времени в строку "hh:mm:ss"
void format_clock(char *buffer, ClockTime t)
{
    buffer[0] = '0' + (t.hh / 10);
    buffer[1] = '0' + (t.hh % 10);
    buffer[2] = ':';
    buffer[3] = '0' + (t.mm / 10);
    buffer[4] = '0' + (t.mm % 10);
    buffer[5] = ':';
    buffer[6] = '0' + (t.ss / 10);
    buffer[7] = '0' + (t.ss % 10);
    buffer[8] = '\0';
}
