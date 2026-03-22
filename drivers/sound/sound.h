#ifndef SOUND_H
#define SOUND_H

#include <stdint.h>
#include <stdbool.h>

typedef enum
{
    SOUND_STATE_OFF,
    SOUND_STATE_ON,
    SOUND_STATE_BEEPING,
} sound_state_t;

typedef struct
{
    const char *name;
    void (*init)(void);
    bool (*detect)(void);
    bool (*play)(uint32_t frequency);
    void (*stop)(void);
    void (*beep)(uint32_t frequency, uint32_t milliseconds);
    sound_state_t (*get_state)(void);
} sound_ops_t;

void sound_register(const sound_ops_t *ops);

void sound_init(void);
bool sound_detect(void);
bool sound_play(uint32_t frequency);
void sound_stop(void);
void sound_beep(uint32_t frequency, uint32_t milliseconds);
sound_state_t sound_get_state(void);

#endif /* SOUND_H */