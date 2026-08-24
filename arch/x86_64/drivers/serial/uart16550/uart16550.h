#ifndef UART16550_H
#define UART16550_H

#include <stdint.h>
#include <stdbool.h>

// Номера стандартных COM-портов.
#define UART_COM1 1
#define UART_COM2 2
#define UART_COM3 3
#define UART_COM4 4

// Регистры UART 16550 (смещения от базового порта, DLAB = 0)
#define UART_REG_DATA 0x00         // RBR (чтение) / THR (запись)
#define UART_REG_INT_ENABLE 0x01   // IER
#define UART_REG_INT_ID_FIFO 0x02  // IIR (чтение) / FCR (запись)
#define UART_REG_LINE_CTRL 0x03    // LCR
#define UART_REG_MODEM_CTRL 0x04   // MCR
#define UART_REG_LINE_STATUS 0x05  // LSR
#define UART_REG_MODEM_STATUS 0x06 // MSR

// Те же смещения при DLAB = 1
#define UART_REG_DIVISOR_LOW 0x00  // DLL
#define UART_REG_DIVISOR_HIGH 0x01 // DLM

// Биты LSR
#define UART_LSR_DATA_READY 0x01
#define UART_LSR_THR_EMPTY 0x20

// Бит DLAB в LCR
#define UART_LCR_DLAB 0x80

// Опорная частота генератора делителя 16550
#define UART_CLOCK_HZ 115200U

// Регистрирует этот backend как активную реализацию serial_ops_t
void uart16550_driver_init(void);

#endif // UART16550_H