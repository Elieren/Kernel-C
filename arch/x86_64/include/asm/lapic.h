#ifndef X86_64_LAPIC_H
#define X86_64_LAPIC_H

#include <stdint.h>
#include <stdbool.h>

#define LAPIC_REG_ID 0x020       /* Local APIC ID (R/W)              */
#define LAPIC_REG_VERSION 0x030  /* Version                         */
#define LAPIC_REG_TPR 0x080      /* Task Priority                   */
#define LAPIC_REG_EOI 0x0B0      /* End Of Interrupt                */
#define LAPIC_REG_SVR 0x0F0      /* Spurious Interrupt Vector       */
#define LAPIC_REG_ESR 0x280      /* Error Status                    */
#define LAPIC_REG_ICR_LOW 0x300  /* Interrupt Command (low dword)   */
#define LAPIC_REG_ICR_HIGH 0x310 /* Interrupt Command (high dword)  */
#define LAPIC_REG_LVT_TIMER 0x320
#define LAPIC_REG_LVT_THERMAL 0x330
#define LAPIC_REG_LVT_PERF 0x340
#define LAPIC_REG_LVT_LINT0 0x350
#define LAPIC_REG_LVT_LINT1 0x360
#define LAPIC_REG_LVT_ERROR 0x370
#define LAPIC_REG_TIMER_INIT 0x380
#define LAPIC_REG_TIMER_CURRENT 0x390
#define LAPIC_REG_TIMER_DIVIDE 0x3E0

#define LAPIC_SVR_ENABLE (1u << 8)
#define LAPIC_SPURIOUS_VECTOR 0xFF

#define LAPIC_ICR_VECTOR(x) ((uint32_t)(x) & 0xFF)
#define LAPIC_ICR_DELIVERY_FIXED (0u << 8)
#define LAPIC_ICR_DELIVERY_INIT (5u << 8)
#define LAPIC_ICR_DELIVERY_SIPI (6u << 8)
#define LAPIC_ICR_DEST_PHYSICAL (0u << 11)
#define LAPIC_ICR_LEVEL_ASSERT (1u << 14)
#define LAPIC_ICR_TRIGGER_LEVEL (1u << 15)
#define LAPIC_ICR_DELIVERY_PENDING (1u << 12)
/* shorthand «все, кроме себя» */
#define LAPIC_ICR_DEST_ALL_BUT_SELF (3u << 18)

#define LAPIC_IPI_RESCHEDULE 0x40    /* перепланировать на этом ядре     */
#define LAPIC_IPI_TLB_SHOOTDOWN 0x41 /* сбросить TLB */
#define LAPIC_IPI_HALT 0x43

/* Инициализация LAPIC на текущем ядре. keep_pic=true оставляет LINT0 в ExtINT,
 * иначе IRQ от 8259 не доходят до CPU. */
bool lapic_init(uint64_t base);
bool lapic_init_extint(uint64_t base, bool keep_pic);

/* x2APIC нужен при APIC ID > 254 (в xAPIC 0xFF — broadcast).
 * lapic_set_x2apic() вызывается на BSP до lapic_init*(); AP читают флаг сами. */
bool lapic_x2apic_supported(void);
void lapic_set_x2apic(bool enable);
bool lapic_is_x2apic(void);

uint32_t lapic_id(void);

void lapic_send_ipi(uint32_t dest_apic_id, uint32_t vector);

void lapic_send_ipi_all_but_self(uint32_t vector);
void lapic_send_init_all_but_self(void);
void lapic_send_init_deassert_all_but_self(void); /* xAPIC: снять level-INIT */

void lapic_send_init(uint32_t dest_apic_id);
void lapic_send_sipi(uint32_t dest_apic_id, uint8_t vector_page);

void lapic_eoi(void);

void lapic_delay_ms(uint32_t ms);

#endif /* X86_64_LAPIC_H */
