#include "uart16550.h"
#include "drivers/serial/serial.h"
#include <asm/io.h>
#include <asm/cpu.h>

static const uint16_t port_addr_table[4] = {
    0x3F8, // COM1
    0x2F8, // COM2
    0x3E8, // COM3
    0x2E8, // COM4
};

// Флаги успешной инициализации портов: 0=COM1, 1=COM2, 2=COM3, 3=COM4
static bool ready_flags[4] = {false, false, false, false};

// Переводит номер порта (UART_COM1..UART_COM4, т.е. 1..4) в индекс массива (0..3).
// Возвращает -1, если номер неизвестен.
static int port_index(uint16_t uart_id)
{
    if (uart_id < 1 || uart_id > 4)
        return -1; // неизвестный порт

    return (int)uart_id - 1;
}

// Переводит номер порта в реальный I/O-адрес.
static uint16_t port_addr(uint16_t uart_id)
{
    int idx = port_index(uart_id);
    if (idx < 0)
        return 0;

    return port_addr_table[idx];
}

// Чтение статусных битов без проверки готовности порта - используется только в self-test
static bool raw_data_available(uint16_t addr)
{
    return (io_read8(addr + UART_REG_LINE_STATUS) & UART_LSR_DATA_READY) != 0;
}

static bool raw_can_write(uint16_t addr)
{
    return (io_read8(addr + UART_REG_LINE_STATUS) & UART_LSR_THR_EMPTY) != 0;
}

static bool uart_is_ready(uint16_t uart_id)
{
    int idx = port_index(uart_id);
    if (idx < 0)
        return false;

    return ready_flags[idx];
}

// Дополнительно проверяют готовность порта перед чтением статуса
static bool uart_data_available(uint16_t uart_id)
{
    if (!uart_is_ready(uart_id))
        return false;

    return raw_data_available(port_addr(uart_id));
}

static bool uart_can_write(uint16_t uart_id)
{
    if (!uart_is_ready(uart_id))
        return false;

    return raw_can_write(port_addr(uart_id));
}

static char uart_read_char(uint16_t uart_id)
{
    // Защита от зависания: порт не готов - возвращаем 0
    if (!uart_is_ready(uart_id))
        return 0;

    uint16_t addr = port_addr(uart_id);

    while (!raw_data_available(addr))
    {
        cpu_relax();
    }

    return (char)io_read8(addr + UART_REG_DATA);
}

static void uart_write_char(uint16_t uart_id, char c)
{
    // Защита от зависания: порт не готов - байт отбрасывается
    if (!uart_is_ready(uart_id))
        return;

    uint16_t addr = port_addr(uart_id);

    while (!raw_can_write(addr))
    {
        cpu_relax();
    }

    io_write8(addr + UART_REG_DATA, (uint8_t)c);
}

// Самопроверка через loopback-режим UART; вызывается до установки готовности порта
static bool uart_self_test(uint16_t addr)
{
    const uint8_t test_byte = 0xAE;

    // MCR: loopback + OUT2 + OUT1 + RTS
    io_write8(addr + UART_REG_MODEM_CTRL, 0x1E);

    io_write8(addr + UART_REG_DATA, test_byte);

    // Ограничиваем число попыток, чтобы не зависнуть на отсутствующем порту
    for (int i = 0; i < 10000; ++i)
    {
        if (raw_data_available(addr))
        {
            uint8_t got = io_read8(addr + UART_REG_DATA);
            io_write8(addr + UART_REG_MODEM_CTRL, 0x0B); // вернуть нормальный режим
            return got == test_byte;
        }

        cpu_relax();
    }

    io_write8(addr + UART_REG_MODEM_CTRL, 0x0B);
    return false;
}

static bool uart_init(uint16_t uart_id, uint32_t baud_rate)
{
    // Работаем только с известными COM-портами
    int idx = port_index(uart_id);
    if (idx < 0)
        return false;

    uint16_t addr = port_addr_table[idx];

    if (baud_rate == 0)
        baud_rate = 115200;

    // Делитель считаем в 32 битах и подрезаем в диапазон 1..65535
    uint32_t raw_divisor = UART_CLOCK_HZ / baud_rate;
    if (raw_divisor < 1)
        raw_divisor = 1;
    else if (raw_divisor > 0xFFFF)
        raw_divisor = 0xFFFF;
    uint16_t divisor = (uint16_t)raw_divisor;

    // Критическая секция: защищает настройку регистров и ready_flags от race condition
    unsigned long flags = save_flags();
    local_irq_disable();

    // Если порт уже готов - не трогаем железо повторно
    if (ready_flags[idx])
    {
        restore_flags(flags);
        return true;
    }

    // 0. Сбрасываем LCR (и DLAB) в известное состояние
    io_write8(addr + UART_REG_LINE_CTRL, 0x00);

    // 1. Отключаем прерывания UART - драйвер работает поллингом
    io_write8(addr + UART_REG_INT_ENABLE, 0x00);

    // 2. Поднимаем DLAB для доступа к регистру делителя
    io_write8(addr + UART_REG_LINE_CTRL, UART_LCR_DLAB);

    // 3. Записываем делитель скорости
    io_write8(addr + UART_REG_DIVISOR_LOW, (uint8_t)(divisor & 0xFF));
    io_write8(addr + UART_REG_DIVISOR_HIGH, (uint8_t)((divisor >> 8) & 0xFF));

    // 4. Опускаем DLAB, формат кадра 8N1
    io_write8(addr + UART_REG_LINE_CTRL, 0x03);

    // 5. Включаем FIFO, очищаем буферы, порог 14 байт
    io_write8(addr + UART_REG_INT_ID_FIFO, 0xC7);

    // 6. Самопроверка через loopback
    bool ok = uart_self_test(addr);

    if (ok)
    {
        // 7. Рабочий режим: поднимаем DTR/RTS/OUT2
        io_write8(addr + UART_REG_MODEM_CTRL, 0x0B);
    }

    ready_flags[idx] = ok;

    restore_flags(flags);

    return ok;
}

static const serial_ops_t uart16550_ops = {
    .name = "uart16550",
    .init = uart_init,
    .is_ready = uart_is_ready,
    .data_available = uart_data_available,
    .can_write = uart_can_write,
    .read_char = uart_read_char,
    .write_char = uart_write_char,
};

void uart16550_driver_init(void)
{
    serial_register(&uart16550_ops);
}