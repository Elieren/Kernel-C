#ifndef TIMER_H
#define TIMER_H

#include <stdint.h>
#include <stdbool.h>

#define PIT_FREQUENCY 1193180U

void init_timer(uint32_t frequency);

#endif