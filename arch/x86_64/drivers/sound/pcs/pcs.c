#include "pcs.h"
#include "drivers/sound/sound.h"
#include <asm/io.h>
#include "kernel/time/timer.h"
#include "lib/graphics/formatting/formatting.h"
#include <stddef.h>

#define PC_SPEAKER_PORT 0x61
#define PIT_CHANNEL2_PORT 0x42
#define PIT_COMMAND_PORT 0x43
#define PIT_ACCESS_LOHI 0xB6
#define SPEAKER_GATE_MASK 0x01
#define SPEAKER_DATA_MASK 0x02

static sound_state_t current_state = SOUND_STATE_OFF;
static uint32_t current_frequency = 0;

static void enable_speaker(void);
static void disable_speaker(void);
static void set_pit_frequency(uint32_t frequency);
bool pcs_detect(void);
void pcs_stop(void);

void pcs_init(void)
{
    disable_speaker();
    current_state = SOUND_STATE_OFF;
    current_frequency = 0;

    if (!pcs_detect())
    {
        kprint(KPRINT_LOG, "PCS: PCS not found");
        return;
    }
}

bool pcs_detect(void)
{
    uint8_t orig = io_read8(PC_SPEAKER_PORT);
    io_write8(PC_SPEAKER_PORT, orig | SPEAKER_GATE_MASK | SPEAKER_DATA_MASK);
    uint8_t test = io_read8(PC_SPEAKER_PORT);
    io_write8(PC_SPEAKER_PORT, orig);
    return (test & (SPEAKER_GATE_MASK | SPEAKER_DATA_MASK)) ==
           (SPEAKER_GATE_MASK | SPEAKER_DATA_MASK);
}

bool pcs_play(uint32_t frequency)
{
    if (frequency < MIN_FREQUENCY || frequency > MAX_FREQUENCY)
        return false;

    if (current_state == SOUND_STATE_ON && current_frequency == frequency)
        return true;

    if (current_state != SOUND_STATE_OFF)
        pcs_stop();

    set_pit_frequency(frequency);
    enable_speaker();

    current_state = SOUND_STATE_ON;
    current_frequency = frequency;
    return true;
}

void pcs_stop(void)
{
    if (current_state == SOUND_STATE_OFF)
        return;

    disable_speaker();
    current_state = SOUND_STATE_OFF;
    current_frequency = 0;
}

void pcs_beep(uint32_t frequency, uint32_t milliseconds)
{
    if (frequency < MIN_FREQUENCY || frequency > MAX_FREQUENCY || milliseconds == 0)
        return;

    if (!pcs_play(frequency))
        return;

    current_state = SOUND_STATE_BEEPING;
    mwait(milliseconds);
    pcs_stop();
}

sound_state_t pcs_get_state(void)
{
    return current_state;
}

static void enable_speaker(void)
{
    uint8_t state = io_read8(PC_SPEAKER_PORT);
    state |= (SPEAKER_GATE_MASK | SPEAKER_DATA_MASK);
    io_write8(PC_SPEAKER_PORT, state);
}

static void disable_speaker(void)
{
    uint8_t state = io_read8(PC_SPEAKER_PORT);
    state &= ~(SPEAKER_GATE_MASK | SPEAKER_DATA_MASK);
    io_write8(PC_SPEAKER_PORT, state);
}

static void set_pit_frequency(uint32_t frequency)
{
    uint32_t divisor = PIT_FREQUENCY / frequency;

    if (divisor < 2)
        divisor = 2;
    if (divisor > 65535)
        divisor = 65535;

    io_write8(PIT_COMMAND_PORT, PIT_ACCESS_LOHI);
    io_write8(PIT_CHANNEL2_PORT, (uint8_t)(divisor & 0xFF));
    io_write8(PIT_CHANNEL2_PORT, (uint8_t)((divisor >> 8) & 0xFF));
}

void play_test_theme(void)
{
    const uint32_t notes[] = {440, 523, 587, 659, 698, 784, 880, 1047};
    const uint32_t durations[] = {250, 250, 500, 250, 250, 500, 250, 250};

    pcs_init();

    for (size_t i = 0; i < sizeof(notes) / sizeof(notes[0]); i++)
        pcs_beep(notes[i], durations[i]);

    pcs_stop();
}

static const sound_ops_t pcs_sound_ops = {
    .name = "pcs",
    .init = pcs_init,
    .detect = pcs_detect,
    .play = pcs_play,
    .stop = pcs_stop,
    .beep = pcs_beep,
    .get_state = pcs_get_state,
};

void pcs_sound_driver_init(void)
{
    sound_register(&pcs_sound_ops);
}