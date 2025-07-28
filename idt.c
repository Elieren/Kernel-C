// idt.c
#include "idt.h"
#include "keyboard/portio.h"
#include "isr.h"
#include "pic.h"

// массив указателей на заглушки 0..31
extern void (*const isr_stubs[32])();
void (*const isr_stubs[32])() = {
    isr_stub_0, isr_stub_1, /* … */ isr_stub_31};

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
    idt_set_gate(32, (uint32_t)isr32, 0x08, 0x8E);

    idt_load((uint32_t)&idtp);
}