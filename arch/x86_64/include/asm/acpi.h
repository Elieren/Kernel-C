#ifndef X86_64_ACPI_H
#define X86_64_ACPI_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#define ACPI_MAX_CPUS 1024

/* Логический процессор из MADT */
typedef struct
{
    uint32_t apic_id;  /* Local APIC ID; для x2APIC — полные 32 бита */
    uint32_t processor_id;
    bool enabled;      /* бит 0 поля flags: ядро разрешено к использованию */
    bool online;       /* выставлено после успешного INIT-SIPI-SIPI */
    bool is_bsp;       /* true для ядра, на котором мы стартовали */
} acpi_cpu_t;

typedef struct
{
    uint64_t lapic_base;             /* физический адрес MMIO Local APIC */
    acpi_cpu_t cpus[ACPI_MAX_CPUS];
    uint32_t cpu_count;              /* всего записей type 0 */
    uint32_t enabled_count;          /* из них enabled */
    bool madt_found;
    uint64_t madt_addr;
    uint8_t madt_flags;              /* bit0 = PCAT_COMPAT (есть 8259) */
} acpi_info_t;

/* Разбирает ACPI (RSDP -> RSDT/XSDT -> MADT).
   rsdp_phys — адрес RSDP из Multiboot2 (0, если нет).
   Возвращает true, если MADT найден. */
bool acpi_init(uint64_t rsdp_phys);

const acpi_info_t *acpi_get_info(void);

/* Физический адрес таблицы по сигнатуре или 0 */
uint64_t acpi_find_table(const char *signature);

void acpi_dump_to_serial(void);

#endif /* X86_64_ACPI_H */
