#ifndef KEYBOARD_H
#define KEYBOARD_H

#include <stdint.h>
#include <stdbool.h>

#define KBD_BUF_SIZE 256
#define KBD_MAX_DRIVERS 8

typedef struct
{
    const char *name; /* "ps2", "usb", ... */

    bool (*init)(void);    /* инициализировать железо         */
    void (*enable)(void);  /* разрешить прерывания            */
    void (*disable)(void); /* запретить прерывания            */
} keyboard_driver_t;

void keyboard_register(const keyboard_driver_t *drv);

void keyboard_push_char(char c);

/* ============================================================
 *  Публичный API
 * ============================================================ */

/* Инициализировать ВСЕ зарегистрированные драйверы */
bool keyboard_init(void);

/* Включить / выключить ВСЕ зарегистрированные драйверы */
void keyboard_enable(void);
void keyboard_disable(void);

/* Работа с общим буфером */
bool keyboard_has_char(void);
char keyboard_getchar(void); /* -1 если пусто */
void keyboard_flush(void);

/* Состояние модификаторов. */
bool keyboard_is_shift_down(void);
bool keyboard_is_ctrl_down(void);
bool keyboard_is_caps_lock(void);

/* Драйвер обновляет модификаторы через эту функцию */
void keyboard_set_modifiers(bool shift, bool ctrl, bool caps);

#endif /* KEYBOARD_H */