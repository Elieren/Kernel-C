#include "keyboard.h"
#include <asm/cpu.h>

/* ---- Реестр драйверов -------------------------------------- */

static const keyboard_driver_t *g_drivers[KBD_MAX_DRIVERS];
static int g_driver_count = 0;

void keyboard_register(const keyboard_driver_t *drv)
{
    if (!drv || g_driver_count >= KBD_MAX_DRIVERS)
        return;
    g_drivers[g_driver_count++] = drv;
}

/* ---- Общий буфер символов ---------------------------------- */

static char g_buf[KBD_BUF_SIZE];
static volatile int g_head = 0;
static volatile int g_tail = 0;

void keyboard_push_char(char c)
{
    unsigned long flags = save_flags();
    int next = (g_head + 1) % KBD_BUF_SIZE;
    if (next != g_tail)
    {
        g_buf[g_head] = c;
        g_head = next;
    }
    restore_flags(flags);
}

/* ---- Состояние модификаторов ------------------------------- */

static bool g_shift = false;
static bool g_ctrl = false;
static bool g_caps = false;

/* Каждый драйвер вызывает это когда меняется состояние модификаторов */
void keyboard_set_modifiers(bool shift, bool ctrl, bool caps)
{
    g_shift = shift;
    g_ctrl = ctrl;
    g_caps = caps;
}

/* ---- Публичный API ----------------------------------------- */

bool keyboard_init(void)
{
    bool ok = false;
    for (int i = 0; i < g_driver_count; i++)
        if (g_drivers[i]->init)
            ok |= g_drivers[i]->init();
    return ok;
}

void keyboard_enable(void)
{
    for (int i = 0; i < g_driver_count; i++)
        if (g_drivers[i]->enable)
            g_drivers[i]->enable();
}

void keyboard_disable(void)
{
    for (int i = 0; i < g_driver_count; i++)
        if (g_drivers[i]->disable)
            g_drivers[i]->disable();
}

bool keyboard_has_char(void)
{
    unsigned long flags = save_flags();
    bool has = (g_head != g_tail);
    restore_flags(flags);
    return has;
}

char keyboard_getchar(void)
{
    unsigned long flags = save_flags();
    if (g_head == g_tail)
    {
        restore_flags(flags);
        return -1;
    }
    char c = g_buf[g_tail];
    g_tail = (g_tail + 1) % KBD_BUF_SIZE;
    restore_flags(flags);
    return c;
}

void keyboard_flush(void)
{
    unsigned long flags = save_flags();
    g_head = g_tail = 0;
    restore_flags(flags);
}

bool keyboard_is_shift_down(void) { return g_shift; }
bool keyboard_is_ctrl_down(void) { return g_ctrl; }
bool keyboard_is_caps_lock(void) { return g_caps; }