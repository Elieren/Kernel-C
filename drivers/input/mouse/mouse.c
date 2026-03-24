#include "mouse.h"
#include <asm/cpu.h>

/* ---- Реестр драйверов -------------------------------------- */

static const mouse_driver_t *g_drivers[MOUSE_MAX_DRIVERS];
static int g_driver_count = 0;

void mouse_register(const mouse_driver_t *drv)
{
    if (!drv || g_driver_count >= MOUSE_MAX_DRIVERS)
        return;
    g_drivers[g_driver_count++] = drv;
}

/* ---- Общее состояние мыши ---------------------------------- */

static mouse_state_t g_state = {0};

static int32_t g_min_x = 0, g_min_y = 0;
static int32_t g_max_x = 1024, g_max_y = 768;

static void clamp(void)
{
    if (g_state.x < g_min_x)
        g_state.x = g_min_x;
    if (g_state.x > g_max_x)
        g_state.x = g_max_x;
    if (g_state.y < g_min_y)
        g_state.y = g_min_y;
    if (g_state.y > g_max_y)
        g_state.y = g_max_y;
}

void mouse_update_move(int16_t dx, int16_t dy)
{
    unsigned long flags = save_flags();
    g_state.delta_x = (int8_t)dx;
    g_state.delta_y = (int8_t)dy;
    g_state.x += dx;
    g_state.y -= dy; /* Y инвертирован */
    clamp();
    restore_flags(flags);
}

void mouse_update_buttons(bool left, bool right, bool middle)
{
    unsigned long flags = save_flags();

    /* Вычисляем события нажатия / отпускания */
    g_state.left_pressed = !g_state.left_button && left;
    g_state.left_released = g_state.left_button && !left;
    g_state.right_pressed = !g_state.right_button && right;
    g_state.right_released = g_state.right_button && !right;
    g_state.middle_pressed = !g_state.middle_button && middle;
    g_state.middle_released = g_state.middle_button && !middle;

    g_state.left_button = left;
    g_state.right_button = right;
    g_state.middle_button = middle;

    restore_flags(flags);
}

/* ---- Публичный API ----------------------------------------- */

bool mouse_init(void)
{
    bool ok = false;
    for (int i = 0; i < g_driver_count; i++)
        if (g_drivers[i]->init)
            ok |= g_drivers[i]->init();
    return ok;
}

void mouse_enable(void)
{
    for (int i = 0; i < g_driver_count; i++)
        if (g_drivers[i]->enable)
            g_drivers[i]->enable();
}

void mouse_disable(void)
{
    for (int i = 0; i < g_driver_count; i++)
        if (g_drivers[i]->disable)
            g_drivers[i]->disable();
}

void mouse_get_state(mouse_state_t *out)
{
    if (!out)
        return;
    unsigned long flags = save_flags();
    *out = g_state;
    restore_flags(flags);
}

void mouse_get_position(int32_t *x, int32_t *y)
{
    unsigned long flags = save_flags();
    if (x)
        *x = g_state.x;
    if (y)
        *y = g_state.y;
    restore_flags(flags);
}

void mouse_set_position(int32_t x, int32_t y)
{
    unsigned long flags = save_flags();
    g_state.x = x;
    g_state.y = y;
    clamp();
    restore_flags(flags);
}

void mouse_set_bounds(int32_t min_x, int32_t min_y,
                      int32_t max_x, int32_t max_y)
{
    unsigned long flags = save_flags();
    g_min_x = min_x;
    g_min_y = min_y;
    g_max_x = max_x;
    g_max_y = max_y;
    clamp();
    restore_flags(flags);
}

void mouse_get_buttons(bool *left, bool *right, bool *middle)
{
    unsigned long flags = save_flags();
    if (left)
        *left = g_state.left_button;
    if (right)
        *right = g_state.right_button;
    if (middle)
        *middle = g_state.middle_button;
    restore_flags(flags);
}

bool mouse_left_pressed(void)
{
    unsigned long f = save_flags();
    bool r = g_state.left_pressed;
    g_state.left_pressed = false;
    restore_flags(f);
    return r;
}

bool mouse_right_pressed(void)
{
    unsigned long f = save_flags();
    bool r = g_state.right_pressed;
    g_state.right_pressed = false;
    restore_flags(f);
    return r;
}

bool mouse_middle_pressed(void)
{
    unsigned long f = save_flags();
    bool r = g_state.middle_pressed;
    g_state.middle_pressed = false;
    restore_flags(f);
    return r;
}

bool mouse_left_released(void)
{
    unsigned long f = save_flags();
    bool r = g_state.left_released;
    g_state.left_released = false;
    restore_flags(f);
    return r;
}

bool mouse_right_released(void)
{
    unsigned long f = save_flags();
    bool r = g_state.right_released;
    g_state.right_released = false;
    restore_flags(f);
    return r;
}

bool mouse_middle_released(void)
{
    unsigned long f = save_flags();
    bool r = g_state.middle_released;
    g_state.middle_released = false;
    restore_flags(f);
    return r;
}

void mouse_clear_events(void)
{
    unsigned long flags = save_flags();
    g_state.left_pressed = g_state.right_pressed = g_state.middle_pressed = false;
    g_state.left_released = g_state.right_released = g_state.middle_released = false;
    restore_flags(flags);
}