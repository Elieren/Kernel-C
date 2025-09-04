#ifndef IDT_H
#define IDT_H
#include <stdint.h>

#define IDT_ENTRIES 256

#define TIMER 32
#define KEYBOARD 33
#define INTERRUPT 0x80

/* 64-bit IDT entry */
struct __attribute__((packed)) idt_entry
{
    uint16_t offset_low;  /* bits 0..15 */
    uint16_t selector;    /* code segment selector */
    uint8_t ist;          /* IST (3 bits) + zero */
    uint8_t type_attr;    /* type and attributes */
    uint16_t offset_mid;  /* bits 16..31 */
    uint32_t offset_high; /* bits 32..63 */
    uint32_t zero;        /* reserved */
};

/* lidt pointer for 64-bit - 16-byte base */
struct __attribute__((packed)) idt_ptr
{
    uint16_t limit;
    uint64_t base;
};

void idt_set_gate(uint8_t num, void (*handler)(), uint16_t sel, uint8_t flags);
void idt_install(void);

/* 64-bit wrapper for lidt implemented in asm */
extern void lidt_load(struct idt_ptr *p);

#endif /* IDT_H */
