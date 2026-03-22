#include "mouse.h"
#include <stddef.h>

static const mouse_ops_t *active = NULL;

void mouse_register(const mouse_ops_t *ops)
{
    active = ops;
}

bool mouse_init(void)
{
    if (!active || !active->init)
        return false;

    return active->init();
}

void mouse_enable(void)
{
    if (active && active->enable)
        active->enable();
}

void mouse_disable(void)
{
    if (active && active->disable)
        active->disable();
}

void mouse_get_state(mouse_state_t *out)
{
    if (active && active->get_state)
        active->get_state(out);
}

void mouse_get_position(int32_t *x, int32_t *y)
{
    if (active && active->get_position)
        active->get_position(x, y);
}

void mouse_set_position(int32_t x, int32_t y)
{
    if (active && active->set_position)
        active->set_position(x, y);
}

void mouse_set_bounds(int32_t min_x, int32_t min_y,
                      int32_t max_x, int32_t max_y)
{
    if (active && active->set_bounds)
        active->set_bounds(min_x, min_y, max_x, max_y);
}

void mouse_get_buttons(bool *left, bool *right, bool *middle)
{
    if (active && active->get_buttons)
        active->get_buttons(left, right, middle);
}

bool mouse_left_pressed(void)
{
    if (!active || !active->left_pressed)
        return false;
    return active->left_pressed();
}

bool mouse_right_pressed(void)
{
    if (!active || !active->right_pressed)
        return false;
    return active->right_pressed();
}

bool mouse_middle_pressed(void)
{
    if (!active || !active->middle_pressed)
        return false;
    return active->middle_pressed();
}

bool mouse_left_released(void)
{
    if (!active || !active->left_released)
        return false;
    return active->left_released();
}

bool mouse_right_released(void)
{
    if (!active || !active->right_released)
        return false;
    return active->right_released();
}

bool mouse_middle_released(void)
{
    if (!active || !active->middle_released)
        return false;
    return active->middle_released();
}

void mouse_clear_events(void)
{
    if (active && active->clear_events)
        active->clear_events();
}

void mouse_handler(void)
{
    if (active && active->irq_handler)
        active->irq_handler();
}