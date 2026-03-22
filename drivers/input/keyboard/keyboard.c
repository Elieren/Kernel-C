#include "keyboard.h"
#include <stddef.h>

static const keyboard_ops_t *active = NULL;

void keyboard_register(const keyboard_ops_t *ops)
{
    active = ops;
}

int keyboard_init(void)
{
    if (!active || !active->init)
        return -1;

    return active->init();
}

void keyboard_enable(void)
{
    if (active && active->enable)
        active->enable();
}

void keyboard_disable(void)
{
    if (active && active->disable)
        active->disable();
}

bool keyboard_has_char(void)
{
    if (!active || !active->has_char)
        return false;

    return active->has_char();
}

char keyboard_getchar(void)
{
    if (!active || !active->getchar)
        return -1;

    return active->getchar();
}

void keyboard_flush(void)
{
    if (active && active->flush)
        active->flush();
}

bool keyboard_is_shift_down(void)
{
    if (!active || !active->is_shift_down)
        return false;

    return active->is_shift_down();
}

bool keyboard_is_ctrl_down(void)
{
    if (!active || !active->is_ctrl_down)
        return false;

    return active->is_ctrl_down();
}

bool keyboard_is_caps_lock(void)
{
    if (!active || !active->is_caps_lock)
        return false;

    return active->is_caps_lock();
}

void keyboard_handler(void)
{
    if (active && active->irq_handler)
        active->irq_handler();
}