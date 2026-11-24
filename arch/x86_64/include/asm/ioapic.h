#ifndef X86_64_IOAPIC_H
#define X86_64_IOAPIC_H

#include <stdint.h>
#include <stdbool.h>

/* Максимальный APIC ID для RTE (0xFF — broadcast) */
#define IOAPIC_MAX_DEST_ID 0xFEu

/* Разбирает I/O APIC из ACPI, MMIO — в user-таблицы страниц.
 * Вызывать до первой user-задачи. */
bool ioapic_init(void);
bool ioapic_present(void);

/* ISA IRQ -> GSI с учётом Interrupt Source Override */
bool ioapic_resolve_isa_irq(uint8_t isa_irq, uint32_t *gsi, bool *active_low, bool *level);

/* Записывает RTE, вход остаётся замаскированным */
bool ioapic_setup_route(uint32_t gsi, uint8_t vector, uint32_t dest_apic_id,
                        bool active_low, bool level);

/* Меняет только назначение, пока линия неактивна */
bool ioapic_set_dest(uint32_t gsi, uint32_t dest_apic_id);

void ioapic_mask(uint32_t gsi);
void ioapic_unmask(uint32_t gsi);

#endif
