#include "ps2_keyboard.h"
#include "drivers/input/keyboard/keyboard.h"
#include "kernel/sched/multitask/multitask.h"
#include <asm/io.h>
#include <asm/pic.h>
#include <asm/cpu.h>

#define INTERNAL_SPACE 0x01

static bool g_shift = false;
static bool g_ctrl = false;
static bool g_caps = false;
static bool g_enabled = false;
static bool g_extended = false; // флаг префикса 0xE0

static bool g_initialized = false;

// Таблица сканкодов -> ASCII
static const char scancode_to_ascii[256] = {
    [KEY_A] = 'a',
    [KEY_B] = 'b',
    [KEY_C] = 'c',
    [KEY_D] = 'd',
    [KEY_E] = 'e',
    [KEY_F] = 'f',
    [KEY_G] = 'g',
    [KEY_H] = 'h',
    [KEY_I] = 'i',
    [KEY_J] = 'j',
    [KEY_K] = 'k',
    [KEY_L] = 'l',
    [KEY_M] = 'm',
    [KEY_N] = 'n',
    [KEY_O] = 'o',
    [KEY_P] = 'p',
    [KEY_Q] = 'q',
    [KEY_R] = 'r',
    [KEY_S] = 's',
    [KEY_T] = 't',
    [KEY_U] = 'u',
    [KEY_V] = 'v',
    [KEY_W] = 'w',
    [KEY_X] = 'x',
    [KEY_Y] = 'y',
    [KEY_Z] = 'z',

    [KEY_1] = '1',
    [KEY_2] = '2',
    [KEY_3] = '3',
    [KEY_4] = '4',
    [KEY_5] = '5',
    [KEY_6] = '6',
    [KEY_7] = '7',
    [KEY_8] = '8',
    [KEY_9] = '9',
    [KEY_0] = '0',

    [KEY_MINUS] = '-',
    [KEY_EQUAL] = '=',
    [KEY_SQUARE_OPEN_BRACKET] = '[',
    [KEY_SQUARE_CLOSE_BRACKET] = ']',
    [KEY_SEMICOLON] = ';',
    [KEY_BACKSLASH] = '\\',
    [KEY_COMMA] = ',',
    [KEY_DOT] = '.',
    [KEY_FORESLHASH] = '/',
    [KEY_APOSTROPHE] = '\'',
    [KEY_GRAVE] = '`',

    [KEY_SPACE] = INTERNAL_SPACE,
    [KEY_ENTER] = '\n',
    [KEY_TAB] = '\t',
    [KEY_BACKSPACE] = '\b',
};

static const char scancode_to_ascii_shifted[256] = {
    [KEY_A] = 'A',
    [KEY_B] = 'B',
    [KEY_C] = 'C',
    [KEY_D] = 'D',
    [KEY_E] = 'E',
    [KEY_F] = 'F',
    [KEY_G] = 'G',
    [KEY_H] = 'H',
    [KEY_I] = 'I',
    [KEY_J] = 'J',
    [KEY_K] = 'K',
    [KEY_L] = 'L',
    [KEY_M] = 'M',
    [KEY_N] = 'N',
    [KEY_O] = 'O',
    [KEY_P] = 'P',
    [KEY_Q] = 'Q',
    [KEY_R] = 'R',
    [KEY_S] = 'S',
    [KEY_T] = 'T',
    [KEY_U] = 'U',
    [KEY_V] = 'V',
    [KEY_W] = 'W',
    [KEY_X] = 'X',
    [KEY_Y] = 'Y',
    [KEY_Z] = 'Z',

    [KEY_1] = '!',
    [KEY_2] = '@',
    [KEY_3] = '#',
    [KEY_4] = '$',
    [KEY_5] = '%',
    [KEY_6] = '^',
    [KEY_7] = '&',
    [KEY_8] = '*',
    [KEY_9] = '(',
    [KEY_0] = ')',

    [KEY_MINUS] = '_',
    [KEY_EQUAL] = '+',
    [KEY_SQUARE_OPEN_BRACKET] = '{',
    [KEY_SQUARE_CLOSE_BRACKET] = '}',
    [KEY_SEMICOLON] = ':',
    [KEY_BACKSLASH] = '|',
    [KEY_COMMA] = '<',
    [KEY_DOT] = '>',
    [KEY_FORESLHASH] = '?',
    [KEY_APOSTROPHE] = '\"',
    [KEY_GRAVE] = '~',

    [KEY_SPACE] = INTERNAL_SPACE,
    [KEY_ENTER] = '\n',
    [KEY_TAB] = '\t',
    [KEY_BACKSPACE] = '\b',
};

// ============================================================================
// Вспомогательные функции
// ============================================================================

static inline char my_toupper(char c)
{
    if (c >= 'a' && c <= 'z')
    {
        return c - 32;
    }
    return c;
}

static bool is_alpha(uint8_t scancode)
{
    switch (scancode)
    {
    case KEY_A:
    case KEY_B:
    case KEY_C:
    case KEY_D:
    case KEY_E:
    case KEY_F:
    case KEY_G:
    case KEY_H:
    case KEY_I:
    case KEY_J:
    case KEY_K:
    case KEY_L:
    case KEY_M:
    case KEY_N:
    case KEY_O:
    case KEY_P:
    case KEY_Q:
    case KEY_R:
    case KEY_S:
    case KEY_T:
    case KEY_U:
    case KEY_V:
    case KEY_W:
    case KEY_X:
    case KEY_Y:
    case KEY_Z:
        return true;
    default:
        return false;
    }
}

static char get_ascii_char(uint8_t scancode)
{
    if (is_alpha(scancode))
    {
        bool upper = g_shift ^ g_caps;
        char base = scancode_to_ascii[scancode]; // 'a'-'z'
        return upper ? my_toupper(base) : base;
    }

    char c = g_shift ? scancode_to_ascii_shifted[scancode]
                     : scancode_to_ascii[scancode];
    return (c == INTERNAL_SPACE) ? ' ' : c;
}

// ============================================================================
// Низкоуровневые функции работы с контроллером клавиатуры
// ============================================================================

static void kbd_wait_input(void)
{
    uint32_t timeout = 100000;
    while (timeout--)
    {
        if (!(io_read8(KEYBOARD_COMMAND_PORT) & KBD_STATUS_INPUT_FULL))
            return;
    }
}

static void kbd_wait_output(void)
{
    uint32_t timeout = 100000;
    while (timeout--)
    {
        if (io_read8(KEYBOARD_COMMAND_PORT) & KBD_STATUS_OUTPUT_FULL)
            return;
    }
}

static void kbd_write_command(uint8_t cmd)
{
    kbd_wait_input();
    io_write8(KEYBOARD_COMMAND_PORT, cmd);
}

static uint8_t kbd_read_data(void)
{
    kbd_wait_output();
    return io_read8(KEYBOARD_DATA_PORT);
}

static void kbd_write_data(uint8_t data)
{
    kbd_wait_input();
    io_write8(KEYBOARD_DATA_PORT, data);
}

// ============================================================================
// Управление LED индикаторами
// ============================================================================

static void keyboard_update_leds(void)
{
    uint8_t led_state = 0;

    if (g_caps)
        led_state |= 0x04;

    // Отправляем команду установки LED
    kbd_write_data(KBD_CMD_SET_LED);
    kbd_read_data(); // ACK
    kbd_write_data(led_state);
    kbd_read_data(); // ACK
}

// ============================================================================
// Публичный API
// ============================================================================

static bool ps2_keyboard_init(void)
{
    if (g_initialized)
        return true;

    // Очистка выходного буфера контроллера
    int clear_attempts = 0;
    while ((io_read8(KEYBOARD_COMMAND_PORT) & KBD_STATUS_OUTPUT_FULL) &&
           clear_attempts++ < 100)
    {
        io_read8(KEYBOARD_DATA_PORT);
        for (volatile int i = 0; i < 1000; i++)
            ;
    }

    // Включить первый PS/2 порт
    kbd_write_command(KBD_CMD_ENABLE_KEYBOARD);

    // Сброс клавиатуры (0xFF)
    kbd_write_data(KBD_CMD_RESET);

    uint8_t response = kbd_read_data();
    if (response != KBD_RESPONSE_ACK)
        return false;

    // Ждём результат самотестирования (BAT)
    response = kbd_read_data();
    if (response != KBD_RESPONSE_BAT_OK)
        return false;

    // Установить параметры по умолчанию (0xF6)
    kbd_write_data(KBD_CMD_SET_DEFAULTS);
    response = kbd_read_data();
    if (response != KBD_RESPONSE_ACK)
        return false;

    // Инициализация состояния
    g_shift = false;
    g_caps = false;
    g_ctrl = false;
    g_enabled = false;
    g_extended = false;
    g_initialized = true;
    return true;
}

static void ps2_keyboard_enable(void)
{
    unsigned long flags = save_flags();
    local_irq_disable();

    if (!g_initialized || g_enabled)
    {
        restore_flags(flags);
        return;
    }

    kbd_write_data(KBD_CMD_ENABLE_SCANNING);
    uint8_t response = kbd_read_data();

    if (response == KBD_RESPONSE_ACK)
    {
        g_enabled = true;
    }
    else
    {
        while (io_read8(KEYBOARD_COMMAND_PORT) & KBD_STATUS_OUTPUT_FULL)
            io_read8(KEYBOARD_DATA_PORT);
    }

    restore_flags(flags);
}

static void ps2_keyboard_disable(void)
{
    unsigned long flags = save_flags();
    local_irq_disable();

    if (!g_initialized || !g_enabled)
    {
        restore_flags(flags);
        return;
    }

    kbd_write_data(KBD_CMD_DISABLE_SCANNING);
    uint8_t response = kbd_read_data();

    if (response == KBD_RESPONSE_ACK)
    {
        g_enabled = false;
    }
    else
    {
        while (io_read8(KEYBOARD_COMMAND_PORT) & KBD_STATUS_OUTPUT_FULL)
            io_read8(KEYBOARD_DATA_PORT);
    }

    restore_flags(flags);
}

// ============================================================================
// Обработчик прерывания
// ============================================================================

void ps2_keyboard_handler(void)
{
    uint8_t status = io_read8(KEYBOARD_COMMAND_PORT);

    // Проверяем, что данные действительно доступны
    if (!(status & KBD_STATUS_OUTPUT_FULL))
    {
        pic_send_eoi(1);
        return;
    }

    // Проверяем, что данные от клавиатуры (бит 5 = 0), а не от мыши (бит 5 = 1)
    if (status & 0x20)
    {
        // Это данные от мыши, не трогаем их
        pic_send_eoi(1);
        return;
    }

    uint8_t code = io_read8(KEYBOARD_DATA_PORT);

    // Префикс расширенного сканкода — запоминаем и ждём следующий байт
    if (code == 0xE0)
    {
        g_extended = true;
        pic_send_eoi(1);
        return;
    }

    bool released = (code & 0x80) != 0;
    uint8_t key = code & 0x7F;

    // Обработка расширенных сканкодов
    if (g_extended)
    {
        g_extended = false;

        if (key == KEY_RCONTROL_EXT) // правый Ctrl: 0xE0 0x1D
        {
            g_ctrl = !released;
            keyboard_set_modifiers(g_shift, g_ctrl, g_caps);
        }
        else if (!released)
        {
            /*
             * Генерируем стандартные ANSI/VT100-последовательности:
             *   ↑  ESC [ A
             *   ↓  ESC [ B
             *   →  ESC [ C
             *   ←  ESC [ D
             */
            if (key == KEY_UP)
            {
                keyboard_push_char(0x1B);
                keyboard_push_char('[');
                keyboard_push_char('A');
            }
            else if (key == KEY_DOWN)
            {
                keyboard_push_char(0x1B);
                keyboard_push_char('[');
                keyboard_push_char('B');
            }
            else if (key == KEY_RIGHT)
            {
                keyboard_push_char(0x1B);
                keyboard_push_char('[');
                keyboard_push_char('C');
            }
            else if (key == KEY_LEFT)
            {
                keyboard_push_char(0x1B);
                keyboard_push_char('[');
                keyboard_push_char('D');
            }
        }
        // Сюда можно добавить KEY_RALT_EXT, KEY_KEYPAD_DIV_EXT и др.

        pic_send_eoi(1);
        return;
    }

    // Обработка обычных модификаторов
    if (key == KEY_LSHIFT || key == KEY_RSHIFT)
    {
        g_shift = !released;
        keyboard_set_modifiers(g_shift, g_ctrl, g_caps);
        pic_send_eoi(1);
        return;
    }

    if (key == KEY_LCONTROL)
    {
        g_ctrl = !released;
        keyboard_set_modifiers(g_shift, g_ctrl, g_caps);
        pic_send_eoi(1);
        return;
    }

    if (key == KEY_CAPSLOCK && !released)
    {
        g_caps = !g_caps;

        // Обновляем LED
        keyboard_update_leds();
        keyboard_set_modifiers(g_shift, g_ctrl, g_caps);

        pic_send_eoi(1);
        return;
    }

    // Обработка нажатия клавиш (только make-коды)
    if (!released && g_enabled)
    {
        // Обработка Ctrl+C
        if (g_ctrl && key == KEY_C)
        {
            task_kill_foreground();
            pic_send_eoi(1);
            return;
        }

        // Преобразование в ASCII
        char ch = get_ascii_char(key);
        if (ch)
        {
            keyboard_push_char(ch);
        }
    }

    pic_send_eoi(1);
}

// ============================================================================
// Таблица драйвера
// ============================================================================

static const keyboard_driver_t ps2_keyboard_driver = {
    .name = "ps2",
    .init = ps2_keyboard_init,
    .enable = ps2_keyboard_enable,
    .disable = ps2_keyboard_disable,
};

// ============================================================================
// Точка входа
// ============================================================================

void ps2_keyboard_register(void)
{
    keyboard_register(&ps2_keyboard_driver);
}