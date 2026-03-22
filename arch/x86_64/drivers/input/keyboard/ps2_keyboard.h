#ifndef PS2_KEYBOARD_H
#define PS2_KEYBOARD_H

#include <stdint.h>
#include <stdbool.h>

// ============================================================================
// Порты клавиатуры
// ============================================================================
#define KEYBOARD_DATA_PORT 0x60
#define KEYBOARD_COMMAND_PORT 0x64

// ============================================================================
// Команды для контроллера клавиатуры
// ============================================================================
#define KBD_CMD_ENABLE_KEYBOARD 0xAE  // Включить первый PS/2 порт
#define KBD_CMD_DISABLE_KEYBOARD 0xAD // Отключить первый PS/2 порт

// ============================================================================
// Команды для самой клавиатуры (отправляются на порт 0x60)
// ============================================================================
#define KBD_CMD_SET_LED 0xED          // Установить состояние LED
#define KBD_CMD_ECHO 0xEE             // Эхо-тест
#define KBD_CMD_SET_SCANCODE_SET 0xF0 // Получить/установить скан-код набор
#define KBD_CMD_IDENTIFY 0xF2         // Идентифицировать клавиатуру
#define KBD_CMD_SET_TYPEMATIC 0xF3    // Установить скорость повтора
#define KBD_CMD_ENABLE_SCANNING 0xF4  // Включить сканирование
#define KBD_CMD_DISABLE_SCANNING 0xF5 // Отключить сканирование (дефолт)
#define KBD_CMD_SET_DEFAULTS 0xF6     // Установить дефолтные параметры
#define KBD_CMD_RESEND 0xFE           // Переслать последний байт
#define KBD_CMD_RESET 0xFF            // Сброс и самотестирование

// ============================================================================
// Биты статуса контроллера (порт 0x64)
// ============================================================================
#define KBD_STATUS_OUTPUT_FULL 0x01  // Данные готовы к чтению
#define KBD_STATUS_INPUT_FULL 0x02   // Контроллер занят
#define KBD_STATUS_SYSTEM 0x04       // Системный флаг
#define KBD_STATUS_COMMAND 0x08      // 1 = команда, 0 = данные
#define KBD_STATUS_TIMEOUT 0x40      // Таймаут
#define KBD_STATUS_PARITY_ERROR 0x80 // Ошибка чётности

// ============================================================================
// Ответы клавиатуры
// ============================================================================
#define KBD_RESPONSE_ACK 0xFA    // Команда принята
#define KBD_RESPONSE_RESEND 0xFE // Повторить команду
#define KBD_RESPONSE_ERROR 0xFC  // Ошибка
#define KBD_RESPONSE_BAT_OK 0xAA // Самотестирование пройдено

// ============================================================================
// Скан-коды клавиш (Scan Code Set 1)
// ============================================================================
#define KEY_ESC 0x01
#define KEY_1 0x02
#define KEY_2 0x03
#define KEY_3 0x04
#define KEY_4 0x05
#define KEY_5 0x06
#define KEY_6 0x07
#define KEY_7 0x08
#define KEY_8 0x09
#define KEY_9 0x0A
#define KEY_0 0x0B
#define KEY_MINUS 0x0C
#define KEY_EQUAL 0x0D
#define KEY_BACKSPACE 0x0E
#define KEY_TAB 0x0F

#define KEY_Q 0x10
#define KEY_W 0x11
#define KEY_E 0x12
#define KEY_R 0x13
#define KEY_T 0x14
#define KEY_Y 0x15
#define KEY_U 0x16
#define KEY_I 0x17
#define KEY_O 0x18
#define KEY_P 0x19
#define KEY_SQUARE_OPEN_BRACKET 0x1A
#define KEY_SQUARE_CLOSE_BRACKET 0x1B
#define KEY_ENTER 0x1C
#define KEY_LCONTROL 0x1D

#define KEY_A 0x1E
#define KEY_S 0x1F
#define KEY_D 0x20
#define KEY_F 0x21
#define KEY_G 0x22
#define KEY_H 0x23
#define KEY_J 0x24
#define KEY_K 0x25
#define KEY_L 0x26
#define KEY_SEMICOLON 0x27
#define KEY_APOSTROPHE 0x28
#define KEY_GRAVE 0x29
#define KEY_LSHIFT 0x2A
#define KEY_BACKSLASH 0x2B

#define KEY_Z 0x2C
#define KEY_X 0x2D
#define KEY_C 0x2E
#define KEY_V 0x2F
#define KEY_B 0x30
#define KEY_N 0x31
#define KEY_M 0x32
#define KEY_COMMA 0x33
#define KEY_DOT 0x34
#define KEY_FORESLHASH 0x35
#define KEY_RSHIFT 0x36
#define KEY_KEYPAD_MUL 0x37
#define KEY_LALT 0x38
#define KEY_SPACE 0x39
#define KEY_CAPSLOCK 0x3A

#define KEY_F1 0x3B
#define KEY_F2 0x3C
#define KEY_F3 0x3D
#define KEY_F4 0x3E
#define KEY_F5 0x3F
#define KEY_F6 0x40
#define KEY_F7 0x41
#define KEY_F8 0x42
#define KEY_F9 0x43
#define KEY_F10 0x44

#define KEY_NUMLOCK 0x45
#define KEY_SCROLLLOCK 0x46
#define KEY_HOME 0x47
#define KEY_UP 0x48
#define KEY_PAGE_UP 0x49
#define KEY_KEYPAD_MINUS 0x4A
#define KEY_LEFT 0x4B
#define KEY_KEYPAD_5 0x4C
#define KEY_RIGHT 0x4D
#define KEY_KEYPAD_PLUS 0x4E
#define KEY_END 0x4F
#define KEY_DOWN 0x50
#define KEY_PAGE_DOWN 0x51
#define KEY_INSERT 0x52
#define KEY_DELETE 0x53

#define KEY_F11 0x57
#define KEY_F12 0x58

// Расширенные скан-коды (префикс 0xE0)
#define KEY_RCONTROL 0xE01D
#define KEY_RALT 0xE038
#define KEY_KEYPAD_DIV 0xE035
#define KEY_PRINT_SCREEN 0xE037

// ============================================================================
// Размер буфера
// ============================================================================
#define KBD_BUF_SIZE 256

#endif
