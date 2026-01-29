#include "mouse.h"
#include <asm/io.h>
#include <asm/pic.h>
#include <asm/cpu.h>
#include "lib/graphics/formatting/formatting.h"

// ============================================================================
// Внутренние переменные
// ============================================================================

static bool initialized = false;
static bool enabled = false;

// Состояние мыши
static volatile mouse_state_t current_state = {
    .x = 0,
    .y = 0,
    .delta_x = 0,
    .delta_y = 0,
    .left_button = false,
    .right_button = false,
    .middle_button = false,
    .left_pressed = false,
    .right_pressed = false,
    .middle_pressed = false,
    .left_released = false,
    .right_released = false,
    .middle_released = false};

// Границы экрана
static int32_t bound_min_x = 0;
static int32_t bound_min_y = 0;
static int32_t bound_max_x = 1024;
static int32_t bound_max_y = 768;

// Буфер пакета данных (3 байта)
static uint8_t mouse_packet[3];
static uint8_t mouse_cycle = 0;

// ============================================================================
// Вспомогательные функции для работы с контроллером PS/2
// ============================================================================

static inline void mouse_wait_input(void)
{
    uint32_t timeout = 100000;
    while (timeout--)
    {
        if (!(io_read8(MOUSE_COMMAND_PORT) & PS2_STATUS_INPUT_FULL))
            return;
    }
}

static inline void mouse_wait_output(void)
{
    uint32_t timeout = 100000;
    while (timeout--)
    {
        if (io_read8(MOUSE_COMMAND_PORT) & PS2_STATUS_OUTPUT_FULL)
            return;
    }
}

static void mouse_write_command(uint8_t cmd)
{
    mouse_wait_input();
    io_write8(MOUSE_COMMAND_PORT, cmd);
}

static uint8_t mouse_read_data(void)
{
    mouse_wait_output();
    return io_read8(MOUSE_DATA_PORT);
}

static void mouse_write_data(uint8_t data)
{
    mouse_wait_input();
    io_write8(MOUSE_COMMAND_PORT, PS2_CMD_SEND_TO_MOUSE);
    mouse_wait_input();
    io_write8(MOUSE_DATA_PORT, data);
}

// ============================================================================
// Инициализация мыши
// ============================================================================

bool mouse_init(void)
{
    if (initialized)
        return true;

    // Очистка буфера контроллера
    int clear_attempts = 0;
    while ((io_read8(MOUSE_COMMAND_PORT) & PS2_STATUS_OUTPUT_FULL) && clear_attempts++ < 100)
    {
        io_read8(MOUSE_DATA_PORT);
        for (volatile int i = 0; i < 1000; i++)
            ;
    }

    // Включаем второй PS/2 порт (мышь)
    mouse_write_command(PS2_CMD_ENABLE_MOUSE);

    // Читаем текущую конфигурацию
    mouse_write_command(PS2_CMD_READ_CONFIG);
    uint8_t config = mouse_read_data();

    // Включаем прерывания от мыши (бит 1) и убираем запрет второго порта (бит 5)
    config |= 0x02;  // Включить прерывания от мыши
    config &= ~0x20; // Разрешить второй порт

    // Записываем новую конфигурацию
    mouse_write_command(PS2_CMD_WRITE_CONFIG);
    mouse_wait_input();
    io_write8(MOUSE_DATA_PORT, config);

    // Сбрасываем мышь
    mouse_write_data(MOUSE_CMD_RESET);
    uint8_t response = mouse_read_data();
    if (response != MOUSE_RESPONSE_ACK)
    {
        return false;
    }

    // Ждём результата самотестирования
    response = mouse_read_data();
    if (response != MOUSE_RESPONSE_SELF_TEST_OK)
    {
        return false;
    }

    // Читаем ID устройства (обычно 0x00)
    mouse_read_data();

    // Устанавливаем значения по умолчанию
    mouse_write_data(MOUSE_CMD_SET_DEFAULTS);
    response = mouse_read_data();
    if (response != MOUSE_RESPONSE_ACK)
    {
        return false;
    }

    // Устанавливаем разрешение (2 counts/mm)
    mouse_write_data(MOUSE_CMD_SET_RESOLUTION);
    mouse_read_data(); // ACK
    mouse_write_data(0x02);
    mouse_read_data(); // ACK

    // Устанавливаем частоту сэмплирования (100 samples/sec)
    mouse_write_data(MOUSE_CMD_SET_SAMPLE_RATE);
    mouse_read_data(); // ACK
    mouse_write_data(100);
    mouse_read_data(); // ACK

    // Инициализация состояния
    current_state.x = bound_max_x / 2;
    current_state.y = bound_max_y / 2;
    mouse_cycle = 0;

    initialized = true;
    return true;
}

// ============================================================================
// Управление мышью
// ============================================================================

void mouse_enable(void)
{
    if (!initialized || enabled)
        return;

    unsigned long flags = save_flags();

    enabled = true;

    // Включаем передачу данных
    mouse_write_data(MOUSE_CMD_ENABLE_STREAMING);
    uint8_t response = mouse_read_data();

    // Если не получили ACK, очищаем буфер
    if (response != MOUSE_RESPONSE_ACK)
    {
        while (io_read8(MOUSE_COMMAND_PORT) & PS2_STATUS_OUTPUT_FULL)
            io_read8(MOUSE_DATA_PORT);
    }

    restore_flags(flags);
}

void mouse_disable(void)
{
    if (!initialized || !enabled)
        return;

    unsigned long flags = save_flags();

    enabled = false;

    // Отключаем передачу данных
    mouse_write_data(MOUSE_CMD_DISABLE_STREAMING);
    uint8_t response = mouse_read_data();

    // Если не получили ACK, очищаем буфер
    if (response != MOUSE_RESPONSE_ACK)
    {
        while (io_read8(MOUSE_COMMAND_PORT) & PS2_STATUS_OUTPUT_FULL)
            io_read8(MOUSE_DATA_PORT);
    }

    restore_flags(flags);
}

// ============================================================================
// Получение состояния мыши
// ============================================================================

void mouse_get_state(mouse_state_t *state)
{
    if (!state)
        return;

    unsigned long flags = save_flags();
    *state = current_state;
    restore_flags(flags);
}

void mouse_get_position(int32_t *x, int32_t *y)
{
    unsigned long flags = save_flags();
    if (x)
    {
        *x = current_state.x;
    }
    if (y)
    {
        *y = current_state.y;
    }
    restore_flags(flags);
}

void mouse_set_position(int32_t x, int32_t y)
{
    unsigned long flags = save_flags();

    // Ограничиваем позицию границами
    if (x < bound_min_x)
    {
        x = bound_min_x;
    }
    if (x > bound_max_x)
    {
        x = bound_max_x;
    }
    if (y < bound_min_y)
    {
        y = bound_min_y;
    }
    if (y > bound_max_y)
    {
        y = bound_max_y;
    }

    current_state.x = x;
    current_state.y = y;

    restore_flags(flags);
}

void mouse_get_buttons(bool *left, bool *right, bool *middle)
{
    unsigned long flags = save_flags();
    if (left)
    {
        *left = current_state.left_button;
    }
    if (right)
    {
        *right = current_state.right_button;
    }
    if (middle)
    {
        *middle = current_state.middle_button;
    }
    restore_flags(flags);
}

// ============================================================================
// События
// ============================================================================

bool mouse_left_pressed(void)
{
    unsigned long flags = save_flags();
    bool result = current_state.left_pressed;
    current_state.left_pressed = false;
    restore_flags(flags);
    return result;
}

bool mouse_right_pressed(void)
{
    unsigned long flags = save_flags();
    bool result = current_state.right_pressed;
    current_state.right_pressed = false;
    restore_flags(flags);
    return result;
}

bool mouse_middle_pressed(void)
{
    unsigned long flags = save_flags();
    bool result = current_state.middle_pressed;
    current_state.middle_pressed = false;
    restore_flags(flags);
    return result;
}

bool mouse_left_released(void)
{
    unsigned long flags = save_flags();
    bool result = current_state.left_released;
    current_state.left_released = false;
    restore_flags(flags);
    return result;
}

bool mouse_right_released(void)
{
    unsigned long flags = save_flags();
    bool result = current_state.right_released;
    current_state.right_released = false;
    restore_flags(flags);
    return result;
}

bool mouse_middle_released(void)
{
    unsigned long flags = save_flags();
    bool result = current_state.middle_released;
    current_state.middle_released = false;
    restore_flags(flags);
    return result;
}

void mouse_clear_events(void)
{
    unsigned long flags = save_flags();
    current_state.left_pressed = false;
    current_state.right_pressed = false;
    current_state.middle_pressed = false;
    current_state.left_released = false;
    current_state.right_released = false;
    current_state.middle_released = false;
    restore_flags(flags);
}

// ============================================================================
// Установка границ
// ============================================================================

void mouse_set_bounds(int32_t min_x, int32_t min_y, int32_t max_x, int32_t max_y)
{
    unsigned long flags = save_flags();
    bound_min_x = min_x;
    bound_min_y = min_y;
    bound_max_x = max_x;
    bound_max_y = max_y;

    // Корректируем текущую позицию, если она вне новых границ
    if (current_state.x < bound_min_x)
        current_state.x = bound_min_x;
    if (current_state.x > bound_max_x)
        current_state.x = bound_max_x;
    if (current_state.y < bound_min_y)
        current_state.y = bound_min_y;
    if (current_state.y > bound_max_y)
        current_state.y = bound_max_y;

    restore_flags(flags);
}

// ============================================================================
// Обработчик прерывания
// ============================================================================

void mouse_handler(void)
{
    uint8_t status = io_read8(MOUSE_COMMAND_PORT);

    // Проверяем, что данные доступны
    if (!(status & PS2_STATUS_OUTPUT_FULL))
    {
        pic_send_eoi(12);
        return;
    }

    // Проверяем, что данные от мыши, а не от клавиатуры
    if (!(status & 0x20))
    {
        // Это данные от клавиатуры, не трогаем их
        pic_send_eoi(12);
        return;
    }

    uint8_t mouse_in = io_read8(MOUSE_DATA_PORT);

    if (!enabled)
    {
        pic_send_eoi(12);
        return;
    }

    // Обработка 3-байтного пакета
    switch (mouse_cycle)
    {
    case 0:
        mouse_packet[0] = mouse_in;
        // Проверяем бит 3 (должен быть всегда 1 для валидного пакета)
        if (!(mouse_in & MOUSE_ALWAYS_ONE))
        {
            pic_send_eoi(12);
            return;
        }
        ++mouse_cycle;
        break;

    case 1:
        mouse_packet[1] = mouse_in;
        ++mouse_cycle;
        break;

    case 2:
        mouse_packet[2] = mouse_in;
        mouse_cycle = 0;

        // Проверяем переполнения
        if ((mouse_packet[0] & MOUSE_X_OVERFLOW) || (mouse_packet[0] & MOUSE_Y_OVERFLOW))
        {
            pic_send_eoi(12);
            return;
        }

        // Обрабатываем пакет
        bool old_left = current_state.left_button;
        bool old_right = current_state.right_button;
        bool old_middle = current_state.middle_button;

        // Обновляем состояние кнопок
        current_state.left_button = mouse_packet[0] & MOUSE_LEFT_BUTTON;
        current_state.right_button = mouse_packet[0] & MOUSE_RIGHT_BUTTON;
        current_state.middle_button = mouse_packet[0] & MOUSE_MIDDLE_BUTTON;

        // Определяем события нажатия/отпускания
        current_state.left_pressed = !old_left && current_state.left_button;
        current_state.left_released = old_left && !current_state.left_button;
        current_state.right_pressed = !old_right && current_state.right_button;
        current_state.right_released = old_right && !current_state.right_button;
        current_state.middle_pressed = !old_middle && current_state.middle_button;
        current_state.middle_released = old_middle && !current_state.middle_button;

        // Вычисляем смещение с учётом знака
        int16_t delta_x = mouse_packet[1];
        int16_t delta_y = mouse_packet[2];

        if (mouse_packet[0] & MOUSE_X_SIGN)
            delta_x |= 0xFF00; // Расширяем знак для отрицательных значений
        if (mouse_packet[0] & MOUSE_Y_SIGN)
            delta_y |= 0xFF00;

        current_state.delta_x = (int8_t)delta_x;
        current_state.delta_y = (int8_t)delta_y;

        // Обновляем абсолютную позицию
        current_state.x += delta_x;
        current_state.y -= delta_y; // Y инвертирован

        // Ограничиваем позицию границами
        if (current_state.x < bound_min_x)
            current_state.x = bound_min_x;
        if (current_state.x > bound_max_x)
            current_state.x = bound_max_x;
        if (current_state.y < bound_min_y)
            current_state.y = bound_min_y;
        if (current_state.y > bound_max_y)
            current_state.y = bound_max_y;

        break;
    }

    pic_send_eoi(12);
}