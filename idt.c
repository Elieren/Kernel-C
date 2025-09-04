// idt.c
#include "idt.h"
#include "portio/portio.h"
#include "isr.h"
#include "pic.h"

// массив указателей на заглушки 0..31
static struct idt_entry idt[IDT_ENTRIES];
static struct idt_ptr idtp;

/* Установить запись IDT для 64-bit handler */
void idt_set_gate(uint8_t num, void (*handler)(), uint16_t sel, uint8_t flags)
{
    uint64_t base = (uint64_t)handler;
    idt[num].offset_low = (uint16_t)(base & 0xFFFF);
    idt[num].selector = sel;
    idt[num].ist = 0;
    idt[num].type_attr = flags;
    idt[num].offset_mid = (uint16_t)((base >> 16) & 0xFFFF);
    idt[num].offset_high = (uint32_t)((base >> 32) & 0xFFFFFFFF);
    idt[num].zero = 0;
}

void idt_install(void)
{
    /* Переназначаем PIC (если нужно) */
    pic_remap(0x20, 0x28);

    idtp.limit = (uint16_t)(sizeof(idt) - 1);
    idtp.base = (uint64_t)&idt; /* 64-bit адрес */

    /* Массив указателей на stubs — удобно для цикла */
    void (*stubs[32])() = {
        isr_stub_0, isr_stub_1, isr_stub_2, isr_stub_3,
        isr_stub_4, isr_stub_5, isr_stub_6, isr_stub_7,
        isr_stub_8, isr_stub_9, isr_stub_10, isr_stub_11,
        isr_stub_12, isr_stub_13, isr_stub_14, isr_stub_15,
        isr_stub_16, isr_stub_17, isr_stub_18, isr_stub_19,
        isr_stub_20, isr_stub_21, isr_stub_22, isr_stub_23,
        isr_stub_24, isr_stub_25, isr_stub_26, isr_stub_27,
        isr_stub_28, isr_stub_29, isr_stub_30, isr_stub_31};

    /* CPU exceptions */
    for (int i = 0; i < 32; ++i)
    {
        idt_set_gate(i, stubs[i], 0x08, 0x8E);
    }

    /* IRQ handlers (timer, keyboard) и системный вызов (DPL=3 -> 0xEE) */
    idt_set_gate(TIMER, isr32, 0x08, 0x8E);
    idt_set_gate(KEYBOARD, isr33, 0x08, 0x8E);
    idt_set_gate(INTERRUPT, isr80, 0x08, 0xEE);

    /* Загружаем IDT через 64-bit ассм-обёртку */
    lidt_load(&idtp);
}