#ifndef RTC_H
#define RTC_H

#include <stdint.h>

#define bool _Bool
#define true 1
#define false 0

#define TIMEZONE_OFFSET 7

void read_rtc_time(uint32_t *hour, uint32_t *minute, uint32_t *second);

#endif
