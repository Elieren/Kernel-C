#ifndef KEYBOARD_H
#define KEYBOARD_H

#include <stdint.h>
#include <stdbool.h>

typedef struct
{
    const char *name;
    int (*init)(void);
    void (*enable)(void);
    void (*disable)(void);
    bool (*has_char)(void);
    char (*getchar)(void);
    void (*flush)(void);
    bool (*is_shift_down)(void);
    bool (*is_ctrl_down)(void);
    bool (*is_caps_lock)(void);
    void (*irq_handler)(void);

} keyboard_ops_t;

void keyboard_register(const keyboard_ops_t *ops);

int keyboard_init(void);

void keyboard_enable(void);
void keyboard_disable(void);

bool keyboard_has_char(void);
char keyboard_getchar(void);
void keyboard_flush(void);

bool keyboard_is_shift_down(void);
bool keyboard_is_ctrl_down(void);
bool keyboard_is_caps_lock(void);

void keyboard_handler(void);

#endif /* KEYBOARD_H */