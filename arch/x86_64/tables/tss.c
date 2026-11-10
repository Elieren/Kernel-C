#include <asm/tss.h>
#include <asm/smp.h>
#include "kernel/panic/panic.h"

extern tss_t tss_buffer;
extern uint64_t gdt[];
extern uint64_t stack64_top;

/* По одному TSS на ядро: у каждого свой rsp0 */
static tss_t *tss_table[MAX_TSS];
static tss_t tss_storage[MAX_TSS];

static uint64_t *gdt_slot(uint32_t cpu)
{
    /* GDT: 5 записей, далее по 16 байт на TSS */
    return &gdt[5 + (uint64_t)cpu * 2];
}

void tss_init_cpu(uint32_t cpu)
{
    if (cpu >= MAX_TSS)
        panic("TSS_CPU_INDEX_OUT_OF_RANGE", true, false);

    tss_t *t = &tss_storage[cpu];
    tss_table[cpu] = t;

    /* Очищаем TSS */
    for (size_t i = 0; i < sizeof(tss_t); i++)
        ((uint8_t *)t)[i] = 0;

    /* временный rsp0, планировщик обновит его */
    t->rsp0 = (uint64_t)&stack64_top;
    t->iopb_offset = sizeof(tss_t);

    /* Формируем 64-bit TSS descriptor согласно Intel manual */
    uint64_t tss_addr = (uint64_t)(uintptr_t)t;
    uint16_t tss_limit = sizeof(tss_t) - 1;

    /* Lower 8 bytes: limit(16) | base_low(24) | access(8) | flags(4) | base_mid(16) */
    uint64_t lower = 0;
    lower |= ((uint64_t)tss_limit & 0xFFFF);    /* bits 0-15: limit */
    lower |= ((tss_addr & 0xFFFFFF) << 16);     /* bits 16-39: base_low */
    lower |= (0x89ULL << 40);                   /* bits 40-47: access */
    lower |= (((tss_addr >> 24) & 0xFF) << 56); /* bits 56-63: base_mid */

    /* Upper 8 bytes: base_high(32) | reserved(32) */
    uint64_t upper = (tss_addr >> 32) & 0xFFFFFFFF;

    uint64_t *slot = gdt_slot(cpu);
    slot[0] = lower;
    slot[1] = upper;
}

void tss_init(void)
{
    tss_init_cpu(0);
    tss_load_for_cpu(0);
}

void tss_load_for_cpu(uint32_t cpu)
{
    uint16_t sel = TSS_SEL(cpu);
    __asm__ volatile("ltr %0" ::"r"(sel));
}

void tss_update_rsp0(uint64_t rsp0)
{
    uint32_t cpu = smp_cpu_index();
    if (cpu < MAX_TSS && tss_table[cpu])
        tss_table[cpu]->rsp0 = rsp0;
}

void tss_set_rsp0_for_cpu(uint32_t cpu, uint64_t rsp0)
{
    if (cpu < MAX_TSS && tss_table[cpu])
        tss_table[cpu]->rsp0 = rsp0;
}