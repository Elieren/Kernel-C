#ifndef X86_64_SMP_H
#define X86_64_SMP_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/* Максимальное число ядер. Должно совпадать с MAX_TSS (tss.h) и MAX_CPUS
 * (kernel.asm); ACPI_MAX_CPUS — не меньше. */
#define SMP_MAX_CPUS 1024

/* Данные ядра. Свой индекс ядро читает как %gs:0, поэтому cpu_index — первое поле. */
typedef struct cpu_local
{
    uint32_t cpu_index;   /* 0..SMP_MAX_CPUS-1, 0 = BSP */
    uint32_t apic_id;     /* Local APIC ID этого ядра */
    void *current_task;   /* задача, исполняющаяся на этом ядре */
    void *idle_stack;     /* стек idle-потока */
    bool online;
    int32_t preempt_count; /* >0 — переключение задач запрещено */
} cpu_local_t;

extern cpu_local_t g_cpu_local[SMP_MAX_CPUS];
extern volatile uint32_t g_cpu_count;      /* сколько ядер уже online */
extern volatile uint32_t g_cpu_started;    /* сколько AP стартовало (для ожидания) */

/* Номер текущего ядра; до smp_init() всегда 0 */
uint32_t smp_cpu_index(void);
cpu_local_t *this_cpu(void);

/* Поднимает все AP, возвращает число online-ядер. Вызывается на BSP. */
uint32_t smp_init(void);

bool smp_is_bsp(void);

uint32_t smp_cpu_count(void);

bool smp_cpu_online(uint32_t cpu);

void smp_broadcast_reschedule(void);

/* Аварийная остановка остальных ядер (путь паники): ядра уходят на ядерный CR3
 * и замирают в `cli; hlt`. Возвращает true, если остановились все за timeout_ms.
 * Ядро с запрещёнными прерываниями IPI не получит — тогда false, и на других
 * ядрах задачи ещё могут исполняться. */
bool smp_halt_others(uint32_t timeout_ms);

void smp_dump_to_serial(void);

/* Точка входа AP (из asm), аргумент — индекс ядра */
void ap_main(uint32_t cpu_index);

void cpu_idle_loop(void);

/* Запрет вытеснения: держатель BKL не должен быть вытеснен.
 * При preempt_count > 0 schedule_from_isr() не переключает задачу. */
void preempt_disable(void);
void preempt_enable(void);
int32_t preempt_count(void);

#endif /* X86_64_SMP_H */
