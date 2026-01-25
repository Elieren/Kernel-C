#ifndef PANIC_H
#define PANIC_H

#include <stdint.h>
#include <stdbool.h>

#include <asm/panic.h>

int panic(const char *error_msg, bool do_reboot, bool can_continue);

#endif
