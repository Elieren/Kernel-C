#include "poweroff.h"
#include "../portio/portio.h"
#include <stdint.h>

#define SLP_EN (1 << 13)

// Минимальные структуры ACPI
typedef struct
{
    char signature[8];
    uint8_t checksum;
    char oem_id[6];
    uint8_t revision;
    uint32_t rsdt_address;
} __attribute__((packed)) rsdp_t;

typedef struct
{
    char signature[4];
    uint32_t length;
    uint8_t _reserved1[76];
    uint32_t pm1a_cnt_blk;
    uint32_t pm1b_cnt_blk;
    uint8_t _reserved2[4];
    uint8_t slp_typa;
    uint8_t slp_typb;
} __attribute__((packed)) fadt_t;

// Поиск RSDP в памяти (0xE0000–0xFFFFF)
static rsdp_t *find_rsdp(void)
{
    for (uintptr_t addr = 0x000E0000; addr < 0x000FFFFF; addr += 16)
    {
        rsdp_t *ptr = (rsdp_t *)addr;
        if (*(uint64_t *)ptr == *(uint64_t *)"RSD PTR ")
            return ptr;
    }
    return 0;
}

// Основная функция выключения
void power_off(void)
{
    rsdp_t *rsdp = find_rsdp();

    if (rsdp)
    {
        // Найдено ACPI, пытаемся использовать FADT
        fadt_t *fadt = (fadt_t *)(uintptr_t)rsdp->rsdt_address;

        uint16_t pm1a = (uint16_t)fadt->pm1a_cnt_blk;
        uint16_t pm1b = (uint16_t)fadt->pm1b_cnt_blk;
        uint8_t s5typ = fadt->slp_typa & 0x7;

        uint16_t val = (s5typ << 10) | SLP_EN;

        outw(pm1a, val);
        if (pm1b)
            outw(pm1b, val);
    }

    // Если ACPI не найдено или выключение не сработало — fallback для QEMU/Bochs
    outw(0x604, 0x2000);

    // CPU ждёт, пока питание отключится
    while (1)
        asm volatile("hlt");
}
