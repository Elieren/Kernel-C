#include "sound.h"
#include <stddef.h>

static const sound_ops_t *active = NULL;

void sound_register(const sound_ops_t *ops)
{
    active = ops;
}

void sound_init(void)
{
    if (active && active->init)
        active->init();
}

bool sound_detect(void)
{
    if (!active || !active->detect)
        return false;

    return active->detect();
}

bool sound_play(uint32_t frequency)
{
    if (!active || !active->play)
        return false;

    return active->play(frequency);
}

void sound_stop(void)
{
    if (active && active->stop)
        active->stop();
}

void sound_beep(uint32_t frequency, uint32_t milliseconds)
{
    if (active && active->beep)
        active->beep(frequency, milliseconds);
}

sound_state_t sound_get_state(void)
{
    if (!active || !active->get_state)
        return SOUND_STATE_OFF;

    return active->get_state();
}