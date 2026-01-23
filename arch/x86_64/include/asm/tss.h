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

/* Инициализирует TSS и записывает дескриптор в GDT */
void tss_init(void);

/* Обновляет RSP0 в TSS при переключении контекста */
void tss_update_rsp0(uint64_t rsp0);

#endif