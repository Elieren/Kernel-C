#include "../vga.h"
#include "kprint.h"
#include "../../libc/string.h"

/**
 * Функция для вывода сообщений с различным уровнем важности
 *
 * @param type    Тип сообщения (определяет цвет вывода)
 * @param msg     Сообщение для вывода
 * @return        0 при успехе, -1 при ошибке
 */
int kprint(const uint8_t type, const char *msg)
{
    if (msg == NULL || strlen(msg) == 0)
    {
        print_string("PRINT ERROR: Message can't be empty\n", RED, BLACK);
        return -1;
    }

    uint8_t fg = WHITE; /* default */
    switch (type)
    {
    case KPRINT_LOG:
        fg = YELLOW;
        break;
    case KPRINT_ERROR:
        fg = RED;
        break;
    case KPRINT_SUCCESS:
        fg = GREEN;
        break;
    case KPRINT_NORMAL:
        fg = WHITE;
        break;
    default:
        print_string("PRINT ERROR: Invalid 'kprint' argument (type)\n", RED, BLACK);
        return -1;
    }

    print_string(msg, fg, BLACK);
    return 0;
}
