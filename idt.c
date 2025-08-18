// idt.c
#include "idt.h"
#include "keyboard/portio.h"
#include "isr.h"
#include "pic.h"

// массив указателей на заглушки 0..31
void (*const isr_stubs[32])() = {
    isr_stub_0, isr_stub_1, isr_stub_2, isr_stub_3,
    isr_stub_4, isr_stub_5, isr_stub_6, isr_stub_7,
    isr_stub_8, isr_stub_9, isr_stub_10, isr_stub_11,
    isr_stub_12, isr_stub_13, isr_stub_14, isr_stub_15,
    isr_stub_16, isr_stub_17, isr_stub_18, isr_stub_19,
    isr_stub_20, isr_stub_21, isr_stub_22, isr_stub_23,
    isr_stub_24, isr_stub_25, isr_stub_26, isr_stub_27,
    isr_stub_28, isr_stub_29, isr_stub_30, isr_stub_31};

static struct idt_entry idt[IDT_ENTRIES];
static struct idt_ptr idtp;

void idt_set_gate(uint8_t num, uint32_t base, uint16_t sel, uint8_t flags)
{
    idt[num].base_low = (uint16_t)(base & 0xFFFF);
    idt[num].sel = sel;
    idt[num].always0 = 0;
    idt[num].flags = flags;
    idt[num].base_high = (uint16_t)((base >> 16) & 0xFFFF);
}

void idt_install(void)
{
    pic_remap(0x20, 0x28);
    idtp.limit = sizeof(idt) - 1;
    idtp.base = (uint32_t)&idt;

    // CPU exceptions
    for (int i = 0; i < 32; i++)
    {
        idt_set_gate(i, (uint32_t)isr_stubs[i], 0x08, 0x8E);
    }
    // PIT IRQ0 → вектор 32
    idt_set_gate(TIMER, (uint32_t)isr32, 0x08, 0x8E);
    idt_set_gate(KEYBOARD, (uint32_t)isr33, 0x08, 0x8E);
    idt_set_gate(INTERRUPT, (uint32_t)isr80, 0x08, 0xEE);

    idt_load((uint32_t)&idtp);
}