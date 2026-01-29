#ifndef MOUSE_H
#define MOUSE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

// ============================================================================
// Порты мыши PS/2
// ============================================================================
#define MOUSE_DATA_PORT 0x60
#define MOUSE_COMMAND_PORT 0x64

// ============================================================================
// Команды для контроллера PS/2 (порт 0x64)
// ============================================================================
#define PS2_CMD_READ_CONFIG 0x20   // Читать конфигурацию
#define PS2_CMD_WRITE_CONFIG 0x60  // Записать конфигурацию
#define PS2_CMD_ENABLE_MOUSE 0xA8  // Включить второй PS/2 порт (мышь)
#define PS2_CMD_DISABLE_MOUSE 0xA7 // Отключить второй PS/2 порт
#define PS2_CMD_SEND_TO_MOUSE 0xD4 // Отправить байт мыши

// ============================================================================
// Команды для мыши (отправляются через порт 0x60 после 0xD4)
// ============================================================================
#define MOUSE_CMD_RESET 0xFF             // Сброс мыши
#define MOUSE_CMD_RESEND 0xFE            // Переслать последний байт
#define MOUSE_CMD_SET_DEFAULTS 0xF6      // Установить значения по умолчанию
#define MOUSE_CMD_DISABLE_STREAMING 0xF5 // Отключить передачу данных
#define MOUSE_CMD_ENABLE_STREAMING 0xF4  // Включить передачу данных
#define MOUSE_CMD_SET_SAMPLE_RATE 0xF3   // Установить частоту сэмплирования
#define MOUSE_CMD_GET_DEVICE_ID 0xF2     // Получить ID устройства
#define MOUSE_CMD_SET_REMOTE_MODE 0xF0   // Установить удалённый режим
#define MOUSE_CMD_SET_WRAP_MODE 0xEE     // Установить wrap mode
#define MOUSE_CMD_RESET_WRAP_MODE 0xEC   // Сбросить wrap mode
#define MOUSE_CMD_READ_DATA 0xEB         // Прочитать данные
#define MOUSE_CMD_SET_STREAM_MODE 0xEA   // Установить потоковый режим
#define MOUSE_CMD_STATUS_REQUEST 0xE9    // Запрос статуса
#define MOUSE_CMD_SET_RESOLUTION 0xE8    // Установить разрешение
#define MOUSE_CMD_SET_SCALING_2_1 0xE7   // Установить масштабирование 2:1
#define MOUSE_CMD_SET_SCALING_1_1 0xE6   // Установить масштабирование 1:1

// ============================================================================
// Ответы мыши
// ============================================================================
#define MOUSE_RESPONSE_ACK 0xFA          // Команда принята
#define MOUSE_RESPONSE_NACK 0xFE         // Команда отклонена, повторить
#define MOUSE_RESPONSE_ERROR 0xFC        // Ошибка
#define MOUSE_RESPONSE_SELF_TEST_OK 0xAA // Самотестирование пройдено
#define MOUSE_RESPONSE_DEVICE_ID 0x00    // ID стандартной PS/2 мыши

// ============================================================================
// Биты статуса контроллера (порт 0x64)
// ============================================================================
#define PS2_STATUS_OUTPUT_FULL 0x01  // Данные готовы к чтению
#define PS2_STATUS_INPUT_FULL 0x02   // Контроллер занят
#define PS2_STATUS_SYSTEM 0x04       // Системный флаг
#define PS2_STATUS_COMMAND 0x08      // 1 = команда, 0 = данные
#define PS2_STATUS_TIMEOUT 0x40      // Таймаут
#define PS2_STATUS_PARITY_ERROR 0x80 // Ошибка чётности

// ============================================================================
// Биты пакета данных мыши (первый байт)
// ============================================================================
#define MOUSE_LEFT_BUTTON 0x01   // Левая кнопка нажата
#define MOUSE_RIGHT_BUTTON 0x02  // Правая кнопка нажата
#define MOUSE_MIDDLE_BUTTON 0x04 // Средняя кнопка нажата
#define MOUSE_ALWAYS_ONE 0x08    // Всегда 1 (для валидации пакета)
#define MOUSE_X_SIGN 0x10        // Знак смещения X (1 = отрицательное)
#define MOUSE_Y_SIGN 0x20        // Знак смещения Y (1 = отрицательное)
#define MOUSE_X_OVERFLOW 0x40    // Переполнение X
#define MOUSE_Y_OVERFLOW 0x80    // Переполнение Y

// ============================================================================
// Структура состояния мыши
// ============================================================================
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

// ============================================================================
// Публичный API
// ============================================================================

// Инициализация и управление
bool mouse_init(void);
void mouse_enable(void);
void mouse_disable(void);

// Получение состояния
void mouse_get_state(mouse_state_t *state);
void mouse_get_position(int32_t *x, int32_t *y);
void mouse_set_position(int32_t x, int32_t y);
void mouse_get_buttons(bool *left, bool *right, bool *middle);

// События (сбрасываются после чтения)
bool mouse_left_pressed(void);
bool mouse_right_pressed(void);
bool mouse_middle_pressed(void);
bool mouse_left_released(void);
bool mouse_right_released(void);
bool mouse_middle_released(void);

// Очистка событий
void mouse_clear_events(void);

// Установка границ экрана (для ограничения курсора)
void mouse_set_bounds(int32_t min_x, int32_t min_y, int32_t max_x, int32_t max_y);

#endif