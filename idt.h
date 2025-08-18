// idt.h
#ifndef IDT_H
#define IDT_H
#include <stdint.h>

#define IDT_ENTRIES 256

#define TIMER 32
#define KEYBOARD 33
#define INTERRUPT 0x80

struct idt_entry
{
    uint16_t base_low;
    uint16_t sel;
    uint8_t always0;
    uint8_t flags;
    uint16_t base_high;
} __attribute__((packed));
struct idt_ptr
{
    uint16_t limit;
    uint32_t base;
} __attribute__((packed));

void idt_set_gate(uint8_t num, uint32_t base, uint16_t sel, uint8_t flags);
void idt_install(void);

extern void idt_load(uint32_t idt_ptr_address);

#endif
