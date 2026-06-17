#include "serial.h"
#include <stddef.h>

static const serial_ops_t *active = NULL;

void serial_register(const serial_ops_t *ops)
{
    active = ops;
}

bool serial_init(uint16_t port, uint32_t baud_rate)
{
    if (!active || !active->init)
        return false;

    return active->init(port, baud_rate);
}

bool serial_is_ready(uint16_t port)
{
    if (!active || !active->is_ready)
        return false;

    return active->is_ready(port);
}

bool serial_data_available(uint16_t port)
{
    if (!active || !active->data_available)
        return false;

    return active->data_available(port);
}

bool serial_can_write(uint16_t port)
{
    if (!active || !active->can_write)
        return false;

    return active->can_write(port);
}

char serial_read_char(uint16_t port)
{
    if (!active || !active->read_char)
        return 0;

    return active->read_char(port);
}

bool serial_try_read_char(uint16_t port, char *out)
{
    // Защита от NULL
    if (!out)
        return false;

    if (!serial_data_available(port))
        return false;

    *out = serial_read_char(port);
    return true;
}

void serial_write_char(uint16_t port, char c)
{
    if (!active || !active->write_char)
        return;

    active->write_char(port, c);
}

void serial_write_string(uint16_t port, const char *str)
{
    if (!str)
        return;

    while (*str)
    {
        if (*str == '\n')
            serial_write_char(port, '\r');

        serial_write_char(port, *str);
        str++;
    }
}