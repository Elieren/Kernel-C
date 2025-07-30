// portio.c
#include "portio.h"

uint8_t inb(uint16_t port)
{
    uint8_t ret;
    // читаем из порта — порт в DX, результат в AL
    asm volatile("inb %[p], %[r]"
                 : [r] "=a"(ret)
                 : [p] "d"(port));
    return ret;
}

void outb(uint16_t port, uint8_t data)
{
    // здесь data в AL, порт в DX
    asm volatile("outb %[d], %[p]"
                 :
                 : [d] "a"(data), [p] "d"(port));
}
