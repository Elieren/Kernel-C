/* smp.c — подъём ядер: топология из ACPI MADT, запуск AP по INIT-SIPI-SIPI
 * через трамплин (0x8000). Тик PIT приходит только на BSP, поэтому он
 * рассылает остальным ядрам IPI RESCHEDULE. */
#include <asm/smp.h>
#include <asm/acpi.h>
#include <asm/lapic.h>
#include <asm/tss.h>
#include <asm/idt.h>
#include <asm/cpu.h>
#include <asm/io.h>
#include "drivers/serial/serial.h"
#include "lib/string/string.h"
#include "lib/graphics/formatting/formatting.h"
#include "lib/sync/spinlock/spinlock.h"
#include "kernel/sched/multitask/multitask.h"
#include "mm/paging/paging.h"
#include "mm/malloc/malloc.h"

#define IA32_GS_BASE 0xC0000101
#define TRAMP_BASE 0x8000
#define AP_STACK_SIZE (16 * 1024)
#define APIC_ID_XAPIC_MAX 254 /* 0xFF в xAPIC — broadcast, адресовать нельзя */

extern char ap_trampoline_start[];
extern char ap_trampoline_end[];
extern uint64_t ap_pml4_slot;
extern uint64_t ap_stack_slot;
extern uint64_t ap_entry_slot;
extern uint64_t ap_index_slot;
extern uint32_t ap_claim_slot;

extern void ap_entry64(void);
extern void reload_cs_kernel(void);
extern uint8_t gdt_desc[];
/* g_cpu_local[0].cpu_index обязан быть по смещению 0: smp_cpu_index()
 * читает его как %gs:0 */
cpu_local_t g_cpu_local[SMP_MAX_CPUS];

volatile uint32_t g_cpu_count = 1;
volatile uint32_t g_cpu_started = 0;

/* Карта online-ядер: бит (i % 32) слова (i / 32) */
#define ONLINE_WORDS ((SMP_MAX_CPUS + 31) / 32)
static volatile uint32_t g_online_bits[ONLINE_WORDS] = {1}; /* cpu0 = BSP */
static volatile int g_smp_gs_ready = 0;
static uint32_t g_apic_ids[SMP_MAX_CPUS];
static uint64_t g_lapic_base = 0;

/* Стеки AP выделяет BSP из кучи до отправки SIPI, только для существующих ядер */
static uint8_t *g_ap_stack[SMP_MAX_CPUS];

static void online_set(uint32_t cpu)
{
    __atomic_fetch_or(&g_online_bits[cpu / 32], 1u << (cpu % 32), __ATOMIC_SEQ_CST);
}

static inline uint64_t slot_addr(uint64_t base, void *sym)
{
    return base + (uint64_t)((char *)sym - ap_trampoline_start);
}

static inline uint64_t rdmsr_u(uint32_t msr)
{
    uint32_t lo, hi;
    __asm__ volatile("rdmsr" : "=a"(lo), "=d"(hi) : "c"(msr));
    return ((uint64_t)hi << 32) | lo;
}

static inline void wrmsr_u(uint32_t msr, uint64_t v)
{
    uint32_t lo = (uint32_t)v, hi = (uint32_t)(v >> 32);
    __asm__ volatile("wrmsr" ::"a"(lo), "d"(hi), "c"(msr));
}

static inline void set_gs_base(void *p)
{
    wrmsr_u(IA32_GS_BASE, (uint64_t)(uintptr_t)p);
}

uint32_t smp_cpu_index(void)
{
    if (!g_smp_gs_ready)
        return 0;
    uint32_t v;
    __asm__ volatile("movl %%gs:0, %0" : "=r"(v));
    return v < SMP_MAX_CPUS ? v : 0;
}

cpu_local_t *this_cpu(void)
{
    return &g_cpu_local[smp_cpu_index()];
}

bool smp_is_bsp(void)
{
    return smp_cpu_index() == 0;
}

uint32_t smp_cpu_count(void)
{
    uint32_t n = g_cpu_count;
    return n ? n : 1;
}

bool smp_cpu_online(uint32_t cpu)
{
    if (cpu >= SMP_MAX_CPUS)
        return false;
    return (__atomic_load_n(&g_online_bits[cpu / 32], __ATOMIC_SEQ_CST) >> (cpu % 32)) & 1u;
}

static volatile uint32_t g_halted_cpus;

/* Вызывается из isr_ipi_halt (IF=0), не возвращается */
void smp_ipi_halt_handler(void)
{
    /* уходим с CR3 задачи: паникующее ядро освободит её таблицы страниц */
    paging_switch(NULL);
    __atomic_add_fetch(&g_halted_cpus, 1, __ATOMIC_SEQ_CST);
    for (;;)
        __asm__ volatile("cli; hlt");
}

bool smp_halt_others(uint32_t timeout_ms)
{
    uint32_t others = smp_cpu_count() - 1;
    if (others == 0)
        return true;

    /* повторный вызов (рекурсивная паника) IPI не шлёт */
    if (__atomic_load_n(&g_halted_cpus, __ATOMIC_SEQ_CST) < others)
        lapic_send_ipi_all_but_self(LAPIC_IPI_HALT);

    for (uint32_t ms = 0; ms <= timeout_ms; ms++)
    {
        if (__atomic_load_n(&g_halted_cpus, __ATOMIC_SEQ_CST) >= others)
            return true;
        lapic_delay_ms(1);
    }
    return __atomic_load_n(&g_halted_cpus, __ATOMIC_SEQ_CST) >= others;
}

/* Один IPI «все, кроме себя» вместо цикла по ядрам на каждом тике */
void smp_broadcast_reschedule(void)
{
    if (g_cpu_count <= 1)
        return;
    lapic_send_ipi_all_but_self(LAPIC_IPI_RESCHEDULE);
}

void ap_main(uint32_t cpu_index)
{
    if (cpu_index >= SMP_MAX_CPUS)
        for (;;)
            halt();

    /* два ядра на одном cpu_index недопустимы */
    if (__atomic_load_n(&g_cpu_local[cpu_index].online, __ATOMIC_SEQ_CST))
        for (;;)
            __asm__ volatile("cli; hlt");

    g_cpu_local[cpu_index].cpu_index = cpu_index;
    /* apic_id записал BSP: до lapic_init() AP может вернуть только 8 бит ID */

    /* на GDT ядра: сначала lgdt, затем перезагрузка CS, и только потом ltr */
    {
        struct __attribute__((packed))
        {
            uint16_t limit;
            uint64_t base;
        } gdtr;
        memcpy(&gdtr, gdt_desc, sizeof(gdtr));
        __asm__ volatile("lgdt %0" ::"m"(gdtr) : "memory");
    }
    reload_cs_kernel();

    /* GS-база строго после reload_cs_kernel(): загрузка сегментного регистра
     * обнуляет GS.base, и smp_cpu_index() всегда вернул бы 0. */
    g_smp_gs_ready = 1;
    set_gs_base(&g_cpu_local[cpu_index]);

    /* Свой TSS. tss_init_cpu() ставит rsp0 на стек BSP — сразу переводим на
     * стек AP, иначе первый IPI испортит стек BSP. */
    tss_init_cpu(cpu_index);
    tss_set_rsp0_for_cpu(cpu_index,
                         (uint64_t)(uintptr_t)(g_ap_stack[cpu_index] + AP_STACK_SIZE));
    tss_load_for_cpu(cpu_index);

    /* IDT общая */
    extern struct idt_ptr *idt_get_ptr(void);
    struct idt_ptr *idtp = idt_get_ptr();
    if (idtp)
        lidt_load(idtp);

    /* AP не используют PIC: LINT0 маскируется */
    lapic_init(g_lapic_base);
    g_cpu_local[cpu_index].apic_id = lapic_id();

    g_cpu_local[cpu_index].online = true;
    online_set(cpu_index);
    __atomic_add_fetch(&g_cpu_started, 1, __ATOMIC_SEQ_CST);
    __atomic_add_fetch(&g_cpu_count, 1, __ATOMIC_SEQ_CST);

    /* Переходим на стек idle-задачи: первое прерывание сохранит кадр на текущем
     * стеке, и он должен совпадать с kstack idle-задачи и rsp0 в TSS. */
    task_t *idle = smp_idle_task(cpu_index);
    if (!idle)
        for (;;)
            halt();

    sched_adopt_idle(cpu_index, idle);
    tss_set_rsp0_for_cpu(cpu_index,
                         (uint64_t)(uintptr_t)idle->kstack + idle->kstack_size);

    /* Смена стека, sti и hlt-цикл — в asm (idle.asm), функция не возвращается */
    extern void smp_idle_enter(void *stack_top) __attribute__((noreturn));
    smp_idle_enter((void *)(uintptr_t)((char *)idle->kstack + idle->kstack_size));
}

static void trampoline_prepare(uint64_t kernel_pml4)
{
    size_t size = (size_t)(ap_trampoline_end - ap_trampoline_start);
    memcpy((void *)(uintptr_t)TRAMP_BASE, ap_trampoline_start, size);

    *(uint64_t *)(uintptr_t)slot_addr(TRAMP_BASE, &ap_pml4_slot) = kernel_pml4;
    *(uint64_t *)(uintptr_t)slot_addr(TRAMP_BASE, &ap_entry_slot) = (uint64_t)(uintptr_t)ap_entry64;
}

/* Значения ap_claim_slot (см. trampoline.asm) */
#define AP_CLAIM_IDLE 0xFFFFFFFFu  /* никого не ждём / запуск отменён */
#define AP_CLAIM_TAKEN 0xFFFFFFFEu /* AP захватил слот и стартует     */

/* Результат start_ap() */
#define AP_START_OK 1        /* ядро online                                  */
#define AP_START_CANCELLED 0 /* не ответило; слот ОТОЗВАН, индекс свободен   */
#define AP_START_STUCK (-1)  /* захватило слот, но не дошло до online        */

static volatile uint32_t *claim_slot(void)
{
    return (volatile uint32_t *)(uintptr_t)slot_addr(TRAMP_BASE, &ap_claim_slot);
}

static bool ap_wait_online(uint32_t cpu_index, uint32_t ms)
{
    for (uint32_t i = 0; i < ms; i++)
    {
        if (__atomic_load_n(&g_cpu_local[cpu_index].online, __ATOMIC_SEQ_CST))
            return true;
        lapic_delay_ms(1);
    }
    return __atomic_load_n(&g_cpu_local[cpu_index].online, __ATOMIC_SEQ_CST);
}

/* Запуск одного AP. Все AP уже получили общий INIT и ждут SIPI. Слоты
 * трамплина общие, поэтому ядра запускаются по одному.
 *
 * Защита от опоздавшего AP — атомарный захват слота:
 *   1. BSP пишет стек/индекс и взводит ap_claim_slot = APIC ID ожидаемого ядра;
 *   2. AP делает lock cmpxchg «мой ID -> TAKEN», иначе паркуется;
 *   3. по таймауту BSP делает cmpxchg «мой ID -> IDLE»: удалось — AP уже не
 *      стартует и индекс свободен; не удалось — AP захватил слот, ждём его.
 * Старт определяется по g_cpu_local[idx].online. */
static int start_ap(uint32_t cpu_index, uint32_t apic_id)
{
    volatile uint32_t *claim = claim_slot();

    *(uint64_t *)(uintptr_t)slot_addr(TRAMP_BASE, &ap_stack_slot) =
        (uint64_t)(uintptr_t)(g_ap_stack[cpu_index] + AP_STACK_SIZE);
    *(uint64_t *)(uintptr_t)slot_addr(TRAMP_BASE, &ap_index_slot) = (uint64_t)cpu_index;
    __atomic_store_n(claim, apic_id, __ATOMIC_SEQ_CST);

    lapic_send_sipi(apic_id, (uint8_t)(TRAMP_BASE >> 12));
    if (ap_wait_online(cpu_index, 100))
        return AP_START_OK;

    lapic_send_sipi(apic_id, (uint8_t)(TRAMP_BASE >> 12));

    /* большой таймаут: при сотнях ядер (особенно в эмуляторе) старт долгий */
    if (ap_wait_online(cpu_index, 2000))
        return AP_START_OK;

    /* таймаут: пробуем отозвать слот */
    uint32_t expect = apic_id;
    if (__atomic_compare_exchange_n(claim, &expect, AP_CLAIM_IDLE, false,
                                    __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST))
        return AP_START_CANCELLED;

    /* AP уже захватил слот и стартует */
    if (ap_wait_online(cpu_index, 5000))
        return AP_START_OK;
    return AP_START_STUCK;
}

uint32_t smp_init(void)
{
    g_cpu_local[0].cpu_index = 0;
    g_smp_gs_ready = 1;
    set_gs_base(&g_cpu_local[0]);

    const acpi_info_t *acpi = acpi_get_info();

    g_lapic_base = acpi->lapic_base ? acpi->lapic_base : 0xFEE00000ULL;

    /* APIC ID > 254 xAPIC адресовать не может — нужен x2APIC */
    uint32_t max_id = 0;
    if (acpi->madt_found)
        for (uint32_t i = 0; i < acpi->cpu_count; i++)
            if (acpi->cpus[i].enabled && acpi->cpus[i].apic_id > max_id)
                max_id = acpi->cpus[i].apic_id;

    if (max_id > APIC_ID_XAPIC_MAX)
    {
        if (lapic_x2apic_supported())
        {
            lapic_set_x2apic(true);
            serial_write_string(1, "[smp] APIC IDs > 254 -> x2APIC mode\n");
        }
        else
            serial_write_string(1, "[smp] APIC IDs > 254 but CPU has no x2APIC: "
                                   "cores with ID > 254 will be skipped\n");
    }

    /* LAPIC MMIO лежит выше RAM: резервируем его в user-таблицах,
     * иначе EOI из user-контекста даст #PF */
    paging_reserve_kernel_mmio(g_lapic_base, 4096);
    /* LINT0 в ExtINT обязателен, иначе IRQ0/IRQ1 от 8259 не доставляются */
    lapic_init_extint(g_lapic_base, true);

    g_cpu_local[0].apic_id = lapic_id();
    g_cpu_local[0].online = true;
    g_apic_ids[0] = g_cpu_local[0].apic_id;

    g_online_bits[0] = 1;
    g_cpu_count = 1;
    g_cpu_started = 0;

    smp_idle_task(0);

    /* AP используют тот же CR3 */
    uint64_t cr3;
    __asm__ volatile("mov %%cr3, %0" : "=r"(cr3));

    trampoline_prepare(cr3);

    if (!acpi->madt_found)
    {
        serial_write_string(1, "[smp] MADT not found -> single CPU only\n");
        return 1;
    }

    uint32_t want = 0;
    for (uint32_t i = 0; i < acpi->cpu_count; i++)
        if (acpi->cpus[i].enabled && acpi->cpus[i].apic_id != g_cpu_local[0].apic_id)
            want++;
    if (!want)
        return 1;

    /* один общий INIT и одна пауза вместо INIT на каждое ядро */
    lapic_send_init_all_but_self();
    lapic_delay_ms(10);
    lapic_send_init_deassert_all_but_self();

    /* BSP может быть не первым в MADT: остальным раздаём индексы 1,2,3... */
    uint32_t next_index = 1;
    uint32_t skipped = 0;
    for (uint32_t i = 0; i < acpi->cpu_count && next_index < SMP_MAX_CPUS; i++)
    {
        const acpi_cpu_t *c = &acpi->cpus[i];
        if (!c->enabled)
            continue;
        if (c->apic_id == g_cpu_local[0].apic_id)
            continue;
        if (c->apic_id > APIC_ID_XAPIC_MAX && !lapic_is_x2apic())
        {
            skipped++;
            continue;
        }

        /* стек и idle-контекст готовим заранее на BSP; стек отозванного запуска переиспользуется */
        if (!g_ap_stack[next_index])
            g_ap_stack[next_index] = (uint8_t *)malloc(AP_STACK_SIZE);
        if (!g_ap_stack[next_index] || !smp_idle_task(next_index))
        {
            if (g_ap_stack[next_index])
                free(g_ap_stack[next_index]);
            g_ap_stack[next_index] = NULL;
            serial_write_string(1, "[smp] out of memory for AP, stopping\n");
            break;
        }

        g_apic_ids[next_index] = c->apic_id;
        g_cpu_local[next_index].cpu_index = next_index;
        g_cpu_local[next_index].apic_id = c->apic_id;

        int rc = start_ap(next_index, c->apic_id);
        if (rc == AP_START_OK)
            next_index++;
        else if (rc == AP_START_CANCELLED)
        {
            /* слот отозван: индекс и стек достаются следующему ядру (индексы идут подряд) */
            serial_write_string(1, "[smp] AP failed to start, skipping\n");
            skipped++;
        }
        else
        {
            /* AP может исполняться на g_ap_stack[idx]: ничего не освобождаем и дальше не запускаем */
            serial_write_string(1, "[smp] AP stuck during start, stopping bring-up\n");
            skipped++;
            break;
        }
    }

    if (skipped)
    {
        char b[64];
        kformat(b, sizeof(b), "[smp] skipped %u CPU(s)\n", (unsigned)skipped);
        serial_write_string(1, b);
    }

    return smp_cpu_count();
}

void smp_dump_to_serial(void)
{
    char buf[128];
    kformat(buf, sizeof(buf), "[smp] online CPUs: %u (%s)\n", (unsigned)smp_cpu_count(),
            lapic_is_x2apic() ? "x2APIC" : "xAPIC");
    serial_write_string(1, buf);

    uint32_t total = smp_cpu_count();
    uint32_t shown = 0;
    for (uint32_t i = 0; i < SMP_MAX_CPUS; i++)
    {
        if (!smp_cpu_online(i))
            continue;
        shown++;
        /* serial медленный: первые 16 и последнее */
        if (total > 20 && shown > 16 && shown != total)
        {
            if (shown == 17)
                serial_write_string(1, "[smp]   ...\n");
            continue;
        }
        kformat(buf, sizeof(buf), "[smp]   cpu%u apic_id=%u %s\n",
                (unsigned)i, (unsigned)g_cpu_local[i].apic_id,
                i == 0 ? "(BSP)" : "(AP)");
        serial_write_string(1, buf);
    }
}

/* Idle-задачи: у каждого ядра своя. Создаются заранее в smp_init() на BSP,
 * потому что schedule_from_isr() вызывает smp_idle_task() из прерывания,
 * а malloc там мог бы дедлокнуться на g_heap_lock. */
/* Выделяются при первом обращении, только для существующих ядер */
#define IDLE_STACK_SIZE (16 * 1024)
static task_t *g_idle_tasks[SMP_MAX_CPUS];

/* kstack/kstack_size должны быть валидны: из них считается TSS.rsp0 */
task_t *smp_idle_task(uint32_t cpu)
{
    /* idle нужен каждому ядру, включая BSP: общий task_t затёр бы кадры друг друга */
    if (cpu >= SMP_MAX_CPUS)
        return NULL;

    if (!g_idle_tasks[cpu])
    {
        task_t *t = (task_t *)malloc(sizeof(task_t));
        uint8_t *stk = (uint8_t *)malloc(IDLE_STACK_SIZE);
        if (!t || !stk)
        {
            if (t)
                free(t);
            if (stk)
                free(stk);
            return NULL;
        }
        memset(t, 0, sizeof(*t));
        t->pid = 0; /* как INIT: не учитывается в списках пользователя */
        t->state = TASK_RUNNING;
        t->name = "IDLE";
        t->cpu = (int)cpu;
        t->cwd_idx = 0;
        t->page_table = NULL;
        t->kstack = stk;
        t->kstack_size = IDLE_STACK_SIZE;
        t->in_ring = 0;
        fpu_state_reset(t->fpu_state);
        g_idle_tasks[cpu] = t;
    }
    return g_idle_tasks[cpu];
}

/* Выбор ядра и изменение его счётчика должны быть неделимы относительно
 * вытеснения: иначе задача успеет мигрировать на другое ядро, и счётчики
 * «потекут». Поэтому на это время локальные прерывания запрещены. */
void preempt_disable(void)
{
    unsigned long flags = save_flags();
    local_irq_disable();
    this_cpu()->preempt_count++;
    restore_flags(flags);
}

void preempt_enable(void)
{
    unsigned long flags = save_flags();
    local_irq_disable();
    this_cpu()->preempt_count--;
    restore_flags(flags);
}

int32_t preempt_count(void)
{
    return this_cpu()->preempt_count;
}
