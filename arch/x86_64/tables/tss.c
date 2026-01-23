#include <asm/tss.h>
#include "kernel/panic/panic.h"

extern tss_t tss_buffer;
extern uint64_t gdt[];
extern uint64_t stack64_top;

static tss_t *tss = NULL;

void tss_init(void)
{
    tss = &tss_buffer;

    if (!tss)
    {
        panic("TSS_ALLOCATION_FAILED", true, false);
    }

    /* Очищаем TSS */
    for (int i = 0; i < sizeof(tss_t); i++)
        ((uint8_t *)tss)[i] = 0;

    /* Инициализируем rsp0 с начального kernel stack */
    tss->rsp0 = (uint64_t)&stack64_top;

    if (tss->rsp0 == 0)
    {
        panic("TSS_INVALID_RSP0", true, false);
    }

    tss->iopb_offset = sizeof(tss_t);

    /* Формируем 64-bit TSS descriptor согласно Intel manual */
    uint64_t tss_addr = (uint64_t)tss;
    uint16_t tss_limit = sizeof(tss_t) - 1;

    /* Lower 8 bytes: limit(16) | base_low(24) | access(8) | flags(4) | base_mid(16) */
    uint64_t lower = 0;
    lower |= ((uint64_t)tss_limit & 0xFFFF);        /* bits 0-15: limit */
    lower |= (((tss_addr) & 0xFFFFFF) << 16);       /* bits 16-39: base_low */
    lower |= (0x89ULL << 40);                       /* bits 40-47: access (P=1, DPL=0, Type=9) */
    lower |= ((((tss_addr) >> 24) & 0xFFFF) << 48); /* bits 48-63: base_mid */

    /* Upper 8 bytes: base_high(32) | reserved(32) */
    uint64_t upper = (tss_addr >> 40) & 0xFFFFFFFF;

    gdt[5] = lower;
    gdt[6] = upper;

    /* Загружаем TSS */
    asm volatile("mov $0x28, %%ax; ltr %%ax" ::: "ax");
}

void tss_update_rsp0(uint64_t rsp0)
{
    if (tss)
        tss->rsp0 = rsp0;
}