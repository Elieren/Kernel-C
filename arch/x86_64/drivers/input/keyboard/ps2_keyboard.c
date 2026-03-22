#include "ps2_keyboard.h"
#include "drivers/input/keyboard/keyboard.h"
#include <asm/io.h>
#include <asm/pic.h>
#include <asm/cpu.h>

#define INTERNAL_SPACE 0x01

static bool shift_down = false;
static bool caps_lock = false;
static bool ctrl_down = false;
static volatile bool can_read_keyboard = false;

// Кольцевой буфер
static char kbd_buf[KBD_BUF_SIZE];
static volatile int kbd_head = 0;
static volatile int kbd_tail = 0;

static bool initialized = false;

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

bool is_alpha(uint8_t scancode)
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
        bool upper = shift_down ^ caps_lock;
        char base = scancode_to_ascii[scancode]; // 'a'-'z'
        return upper ? my_toupper(base) : base;
    }

    return shift_down ? scancode_to_ascii_shifted[scancode]
                      : scancode_to_ascii[scancode];
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
// Работа с буфером
// ============================================================================

static void kbd_buffer_push(char c)
{
    unsigned long flags = save_flags();

    int next = (kbd_head + 1) % KBD_BUF_SIZE;
    if (next != kbd_tail)
    {
        kbd_buf[kbd_head] = c;
        kbd_head = next;
    }
    // Иначе буфер полный - символ теряется

    restore_flags(flags);
}

// ============================================================================
// Управление LED индикаторами
// ============================================================================

static void keyboard_update_leds(void)
{
    uint8_t led_state = 0;

    if (caps_lock)
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

void ps2_keyboard_init(void)
{
    if (initialized)
        return;

    int clear_attempts = 0;
    while ((io_read8(KEYBOARD_COMMAND_PORT) & KBD_STATUS_OUTPUT_FULL) && clear_attempts++ < 100)
    {
        io_read8(KEYBOARD_DATA_PORT);
        // Микрозадержка для стабильности
        for (volatile int i = 0; i < 1000; i++)
            ;
    }

    shift_down = false;
    caps_lock = false;
    ctrl_down = false;
    kbd_head = 0;
    kbd_tail = 0;
    can_read_keyboard = true;

    initialized = true;
}

void ps2_keyboard_enable(void)
{
    if (!initialized || can_read_keyboard)
        return;

    unsigned long flags = save_flags();

    can_read_keyboard = true;

    // Включаем сканирование с проверкой
    kbd_write_data(KBD_CMD_ENABLE_SCANNING);
    uint8_t response = kbd_read_data();

    // Если не получили ACK, очищаем буфер
    if (response != KBD_RESPONSE_ACK)
    {
        while (io_read8(KEYBOARD_COMMAND_PORT) & KBD_STATUS_OUTPUT_FULL)
            io_read8(KEYBOARD_DATA_PORT);
    }

    restore_flags(flags);
}

void ps2_keyboard_disable(void)
{
    if (!initialized || !can_read_keyboard)
        return;

    unsigned long flags = save_flags();

    can_read_keyboard = false;

    // Отключаем сканирование с проверкой
    kbd_write_data(KBD_CMD_DISABLE_SCANNING);
    uint8_t response = kbd_read_data();

    // Если не получили ACK, очищаем буфер
    if (response != KBD_RESPONSE_ACK)
    {
        while (io_read8(KEYBOARD_COMMAND_PORT) & KBD_STATUS_OUTPUT_FULL)
            io_read8(KEYBOARD_DATA_PORT);
    }

    restore_flags(flags);
}

bool ps2_keyboard_has_char(void)
{
    unsigned long flags = save_flags();
    bool has = (kbd_head != kbd_tail);
    restore_flags(flags);
    return has;
}

char ps2_kbd_getchar(void)
{
    unsigned long flags = save_flags();

    if (!can_read_keyboard || kbd_head == kbd_tail)
    {
        restore_flags(flags);
        return -1;
    }

    char c = kbd_buf[kbd_tail];
    kbd_tail = (kbd_tail + 1) % KBD_BUF_SIZE;

    restore_flags(flags);
    return c;
}

void ps2_keyboard_flush_buffer(void)
{
    unsigned long flags = save_flags();
    kbd_head = 0;
    kbd_tail = 0;
    restore_flags(flags);
}

// Получение состояния модификаторов
bool ps2_keyboard_is_shift_down(void)
{
    return shift_down;
}

bool ps2_keyboard_is_ctrl_down(void)
{
    return ctrl_down;
}

bool ps2_keyboard_is_caps_lock_on(void)
{
    return caps_lock;
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

    // Проверяем Break-код (бит 7 = 1)
    bool released = code & 0x80;
    uint8_t key = code & 0x7F;

    // Обработка модификаторов
    if (key == KEY_LSHIFT || key == KEY_RSHIFT)
    {
        shift_down = !released;
        pic_send_eoi(1);
        return;
    }

    if (key == KEY_LCONTROL || key == KEY_RCONTROL)
    {
        ctrl_down = !released;
        pic_send_eoi(1);
        return;
    }

    if (key == KEY_CAPSLOCK && !released)
    {
        caps_lock = !caps_lock;

        // Обновляем LED
        keyboard_update_leds();

        pic_send_eoi(1);
        return;
    }

    // Обработка нажатия клавиш (только make-коды)
    if (!released && can_read_keyboard)
    {
        // Обработка Ctrl+C
        if (ctrl_down && key == KEY_C)
        {
            kbd_buffer_push(0x03); // ASCII код для Ctrl+C
            pic_send_eoi(1);
            return;
        }

        // Преобразование в ASCII
        char ch = get_ascii_char(key);
        if (ch)
        {
            kbd_buffer_push(ch);
        }
    }

    pic_send_eoi(1);
}

static const keyboard_ops_t ps2_keyboard_ops = {
    .name = "ps2",
    .init = ps2_keyboard_init,
    .enable = ps2_keyboard_enable,
    .disable = ps2_keyboard_disable,
    .has_char = ps2_keyboard_has_char,
    .getchar = ps2_kbd_getchar,
    .flush = ps2_keyboard_flush_buffer,
    .is_shift_down = ps2_keyboard_is_shift_down,
    .is_ctrl_down = ps2_keyboard_is_ctrl_down,
    .is_caps_lock = ps2_keyboard_is_caps_lock_on,
    .irq_handler = ps2_keyboard_handler,
};

void ps2_keyboard_driver_init(void)
{
    keyboard_register(&ps2_keyboard_ops);
}