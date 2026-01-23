// pic.c
#include <asm/io.h>
#include <asm/pic.h>

#define ICW1_INIT 0x10
#define ICW1_ICW4 0x01
#define ICW4_8086 0x01

void pic_remap(int offset1, int offset2)
{
    // ICW1: инициализация
    io_write8(PIC1_COMMAND, ICW1_INIT | ICW1_ICW4);
    io_write8(PIC2_COMMAND, ICW1_INIT | ICW1_ICW4);

    // ICW2: базовые векторы прерываний
    io_write8(PIC1_DATA, offset1);
    io_write8(PIC2_DATA, offset2);

    // ICW3: каскадная конфигурация
    io_write8(PIC1_DATA, 4); // PIC2 подключен к IRQ2
    io_write8(PIC2_DATA, 2); // PIC2 находится на IRQ2

    // ICW4: режим 8086
    io_write8(PIC1_DATA, ICW4_8086);
    io_write8(PIC2_DATA, ICW4_8086);
}

void pic_send_eoi(uint8_t irq)
{
    if (irq >= 8)
        io_write8(PIC2_COMMAND, 0x20);
    io_write8(PIC1_COMMAND, 0x20);
}
