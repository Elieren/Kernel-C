#ifndef PCS_H
#define PCS_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#define MAX_FREQUENCY 20000
#define MIN_FREQUENCY 20

typedef enum
{
    SPEAKER_OFF,
    SPEAKER_ON,
    SPEAKER_BEEPING
} speaker_state_t;

void pc_speaker_init(void);
bool pc_speaker_play(uint32_t frequency);
void pc_speaker_stop(void);
void pc_speaker_beep(uint32_t frequency, uint32_t milliseconds);
speaker_state_t pc_speaker_get_state(void);
bool pc_speaker_detect(void);
void play_fnaf_main_theme(void);

#endif
