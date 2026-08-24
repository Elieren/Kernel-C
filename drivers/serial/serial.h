#ifndef SERIAL_H
#define SERIAL_H

#include <stdint.h>
#include <stdbool.h>

typedef struct
{
    const char *name;

    // Инициализирует порт на заданной скорости, возвращает false при ошибке самопроверки.
    // Повторный вызов на уже готовом порте не должен заново трогать железо.
    bool (*init)(uint16_t port, uint32_t baud_rate);

    // Был ли init() успешным для этого порта
    bool (*is_ready)(uint16_t port);

    // Есть ли непрочитанный байт во входном буфере
    bool (*data_available)(uint16_t port);

    // Свободен ли передатчик порта
    bool (*can_write)(uint16_t port);

    // Блокирующее чтение одного байта
    char (*read_char)(uint16_t port);

    // Блокирующая запись одного байта
    void (*write_char)(uint16_t port, char c);
} serial_ops_t;

// Регистрирует backend как активную реализацию
void serial_register(const serial_ops_t *ops);

bool serial_init(uint16_t port, uint32_t baud_rate);
bool serial_is_ready(uint16_t port);
bool serial_data_available(uint16_t port);
bool serial_can_write(uint16_t port);

char serial_read_char(uint16_t port);

// Неблокирующее чтение: true и байт в *out, если он был доступен
bool serial_try_read_char(uint16_t port, char *out);

void serial_write_char(uint16_t port, char c);

// Пишет строку, автоматически добавляя '\r' перед '\n'
void serial_write_string(uint16_t port, const char *str);

#endif // SERIAL_H