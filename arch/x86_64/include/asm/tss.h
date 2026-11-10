#ifndef TSS_H
#define TSS_H

#include <stdint.h>
#include <stddef.h>

/* TSS структура для 64-bit режима */
typedef struct
{
    uint32_t reserved0;
    uint64_t rsp0; /* Stack pointer for ring 0 */
    uint64_t rsp1;
    uint64_t rsp2;
    uint64_t reserved1;
    uint64_t ist1;
    uint64_t ist2;
    uint64_t ist3;
    uint64_t ist4;
    uint64_t ist5;
    uint64_t ist6;
    uint64_t ist7;
    uint64_t reserved2;
    uint16_t reserved3;
    uint16_t iopb_offset;
} __attribute__((packed)) tss_t;

/* Селектор TSS ядра: 0x28 + i*16 */
#define TSS_SEL_BASE 0x28
#define TSS_SEL(cpu) ((uint16_t)(TSS_SEL_BASE + (cpu) * 16))
/* Должно совпадать с MAX_CPUS в kernel.asm и SMP_MAX_CPUS */
#define MAX_TSS 1024

/* Инициализирует TSS ядра cpu и пишет дескриптор в GDT (без ltr) */
void tss_init_cpu(uint32_t cpu);

void tss_init(void);

void tss_load_for_cpu(uint32_t cpu);

/* RSP0 в TSS текущего ядра (вызывается планировщиком) */
void tss_update_rsp0(uint64_t rsp0);

void tss_set_rsp0_for_cpu(uint32_t cpu, uint64_t rsp0);

#endif
