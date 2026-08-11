// clock.c

#include "clock.h"
#include <asm/hwclock.h>

volatile ClockTime system_clock = {0, 0, 0}; // старт с 00:00:00
volatile ClockDate system_date = {1, 1, 2000};

void init_system_clock(void)
{
    uint32_t h, m, s;
    read_rtc_time(&h, &m, &s);
    system_clock.hh = (uint8_t)h;
    system_clock.mm = (uint8_t)m;
    system_clock.ss = (uint8_t)s;

    init_system_date();
}

void init_system_date(void)
{
    uint32_t d, mo, y;
    read_rtc_date(&d, &mo, &y);
    system_date.day = (uint8_t)d;
    system_date.month = (uint8_t)mo;
    system_date.year = (uint16_t)y;
}

static uint8_t days_in_month(uint8_t month, uint16_t year)
{
    static const uint8_t days_table[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};

    if (month == 2)
    {
        bool leap = ((year % 4 == 0) && (year % 100 != 0)) || (year % 400 == 0);
        return leap ? 29 : 28;
    }

    if (month >= 1 && month <= 12)
        return days_table[month - 1];

    return 31; // защитное значение на случай некорректных данных
}

void clock_date_tick(void)
{
    system_date.day++;

    if (system_date.day > days_in_month(system_date.month, system_date.year))
    {
        system_date.day = 1;
        system_date.month++;

        if (system_date.month > 12)
        {
            system_date.month = 1;
            system_date.year++;
        }
    }
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

                clock_date_tick();
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

void format_date(char *buffer, ClockDate d)
{
    buffer[0] = '0' + (d.day / 10);
    buffer[1] = '0' + (d.day % 10);
    buffer[2] = '.';
    buffer[3] = '0' + (d.month / 10);
    buffer[4] = '0' + (d.month % 10);
    buffer[5] = '.';
    buffer[6] = '0' + (uint8_t)((d.year / 1000) % 10);
    buffer[7] = '0' + (uint8_t)((d.year / 100) % 10);
    buffer[8] = '0' + (uint8_t)((d.year / 10) % 10);
    buffer[9] = '0' + (uint8_t)(d.year % 10);
    buffer[10] = '\0';
}
