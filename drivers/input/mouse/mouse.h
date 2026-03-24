#ifndef MOUSE_H
#define MOUSE_H

#include <stdint.h>
#include <stdbool.h>

#define MOUSE_BUF_SIZE 64
#define MOUSE_MAX_DRIVERS 4

typedef struct
{
    int32_t x;            // Абсолютная позиция X
    int32_t y;            // Абсолютная позиция Y
    int8_t delta_x;       // Последнее смещение X
    int8_t delta_y;       // Последнее смещение Y
    bool left_button;     // Состояние левой кнопки
    bool right_button;    // Состояние правой кнопки
    bool middle_button;   // Состояние средней кнопки
    bool left_pressed;    // Левая кнопка была нажата
    bool right_pressed;   // Правая кнопка была нажата
    bool middle_pressed;  // Средняя кнопка была нажата
    bool left_released;   // Левая кнопка была отпущена
    bool right_released;  // Правая кнопка была отпущена
    bool middle_released; // Средняя кнопка была отпущена
} mouse_state_t;

typedef struct
{
    const char *name;

    bool (*init)(void);
    void (*enable)(void);
    void (*disable)(void);
} mouse_driver_t;

void mouse_register(const mouse_driver_t *drv);

void mouse_update_move(int16_t dx, int16_t dy);
void mouse_update_buttons(bool left, bool right, bool middle);

/* ============================================================
 *  Публичный API
 * ============================================================ */

bool mouse_init(void);
void mouse_enable(void);
void mouse_disable(void);

void mouse_get_state(mouse_state_t *out);
void mouse_get_position(int32_t *x, int32_t *y);
void mouse_set_position(int32_t x, int32_t y);
void mouse_set_bounds(int32_t min_x, int32_t min_y,
                      int32_t max_x, int32_t max_y);
void mouse_get_buttons(bool *left, bool *right, bool *middle);

bool mouse_left_pressed(void);
bool mouse_right_pressed(void);
bool mouse_middle_pressed(void);
bool mouse_left_released(void);
bool mouse_right_released(void);
bool mouse_middle_released(void);
void mouse_clear_events(void);

#endif /* MOUSE_H */