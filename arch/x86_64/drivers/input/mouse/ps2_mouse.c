#include "ps2_mouse.h"
#include "drivers/input/mouse/mouse.h"
#include <asm/io.h>
#include <asm/pic.h>
#include <asm/cpu.h>

// ============================================================================
// Внутренние переменные
// ============================================================================

static uint8_t g_packet[3];
static uint8_t g_packet_idx = 0;
static bool g_prev_left = false;
static bool g_prev_right = false;
static bool g_prev_middle = false;
static bool g_initialized = false;
static bool g_enabled = false;

// ============================================================================
// Вспомогательные функции для работы с контроллером PS/2
// ============================================================================

static inline void mouse_wait_input(void)
{
    for (uint32_t i = 100000; i; i--)
        if (!(io_read8(MOUSE_COMMAND_PORT) & PS2_STATUS_INPUT_FULL))
        {
            return;
        }
}

static inline void mouse_wait_output(void)
{
    for (uint32_t i = 100000; i; i--)
        if (io_read8(MOUSE_COMMAND_PORT) & PS2_STATUS_OUTPUT_FULL)
        {
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

static bool ps2_mouse_init(void)
{
    if (g_initialized)
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

    g_packet_idx = 0;

    g_initialized = true;
    return true;
}

// ============================================================================
// Управление мышью
// ============================================================================

static void ps2_mouse_enable(void)
{
    unsigned long flags = save_flags();
    local_irq_disable();

    if (!g_initialized || g_enabled)
    {
        restore_flags(flags);
        return;
    }

    mouse_write_data(MOUSE_CMD_ENABLE_STREAMING);
    uint8_t response = mouse_read_data();

    if (response == MOUSE_RESPONSE_ACK)
    {
        g_enabled = true;
    }
    else
    {
        while (io_read8(MOUSE_COMMAND_PORT) & PS2_STATUS_OUTPUT_FULL)
            io_read8(MOUSE_DATA_PORT);
    }

    restore_flags(flags);
}

static void ps2_mouse_disable(void)
{
    unsigned long flags = save_flags();
    local_irq_disable();

    if (!g_initialized || !g_enabled)
    {
        restore_flags(flags);
        return;
    }

    mouse_write_data(MOUSE_CMD_DISABLE_STREAMING);
    uint8_t response = mouse_read_data();

    if (response == MOUSE_RESPONSE_ACK)
    {
        g_enabled = false;
    }
    else
    {
        while (io_read8(MOUSE_COMMAND_PORT) & PS2_STATUS_OUTPUT_FULL)
            io_read8(MOUSE_DATA_PORT);
    }

    restore_flags(flags);
}

// ============================================================================
// Обработчик прерывания
// ============================================================================

void ps2_mouse_handler(void)
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

    uint8_t byte = io_read8(MOUSE_DATA_PORT);

    if (!g_enabled)
    {
        pic_send_eoi(12);
        return;
    }

    // Обработка 3-байтного пакета
    switch (g_packet_idx)
    {
    case 0:
        // Проверяем бит 3 (должен быть всегда 1 для валидного пакета)
        if (!(byte & MOUSE_ALWAYS_ONE))
        {
            pic_send_eoi(12);
            return;
        }
        g_packet[0] = byte;
        g_packet_idx = 1;
        break;

    case 1:
        g_packet[1] = byte;
        g_packet_idx = 2;
        break;

    case 2:
        g_packet[2] = byte;
        g_packet_idx = 0;

        // Проверяем переполнения
        if ((g_packet[0] & MOUSE_X_OVERFLOW) || (g_packet[0] & MOUSE_Y_OVERFLOW))
        {
            pic_send_eoi(12);
            return;
        }

        // Обрабатываем кнопки
        bool left = (g_packet[0] & MOUSE_LEFT_BUTTON) != 0;
        bool right = (g_packet[0] & MOUSE_RIGHT_BUTTON) != 0;
        bool middle = (g_packet[0] & MOUSE_MIDDLE_BUTTON) != 0;

        // Обновляем кнопки в общем состоянии
        if (left != g_prev_left || right != g_prev_right || middle != g_prev_middle)
        {
            mouse_update_buttons(left, right, middle);
            g_prev_left = left;
            g_prev_right = right;
            g_prev_middle = middle;
        }

        // Обновляем позицию в общем состоянии
        int16_t dx = g_packet[1];
        int16_t dy = g_packet[2];

        if (g_packet[0] & MOUSE_X_SIGN)
            dx |= 0xFF00; // Расширяем знак для отрицательных значений
        if (g_packet[0] & MOUSE_Y_SIGN)
            dy |= 0xFF00;

        if (dx != 0 || dy != 0)
            mouse_update_move(dx, -dy);

        break;
    }

    pic_send_eoi(12);
}

// ============================================================================
// Таблица драйвера
// ============================================================================

static const mouse_driver_t ps2_mouse_driver = {
    .name = "ps2",
    .init = ps2_mouse_init,
    .enable = ps2_mouse_enable,
    .disable = ps2_mouse_disable,
};

// ============================================================================
// Точка входа
// ============================================================================

void ps2_mouse_register(void)
{
    mouse_register(&ps2_mouse_driver);
}