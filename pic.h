// pic.h
#ifndef PIC_H
#define PIC_H
#include <stdint.h>
#define PIC1_COMMAND 0x20
#define PIC1_DATA 0x21
#define PIC2_COMMAND 0xA0
#define PIC2_DATA 0xA1
void pic_remap(int offset1, int offset2);
void pic_send_eoi(uint8_t irq);
#endif