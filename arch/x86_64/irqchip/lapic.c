/* lapic.c — драйвер Local APIC (xAPIC через MMIO, x2APIC через MSR). */
#include <asm/lapic.h>
#include <asm/io.h>
#include <asm/cpu.h>

#define LAPIC_BASE_MSR 0x1B
#define LAPIC_BASE_EN (1ULL << 11)   /* глобальное разрешение APIC   */
#define LAPIC_BASE_EXTD (1ULL << 10) /* режим x2APIC                 */
#define X2APIC_MSR_BASE 0x800        /* регистр reg -> MSR 0x800 + reg/16 */
#define X2APIC_MSR_ICR 0x830         /* ICR — один 64-битный регистр */

static volatile uint32_t *g_lapic = 0;
static uint64_t g_lapic_phys = 0;
static volatile bool g_x2apic = false;

static inline uint64_t rdmsr_raw(uint32_t msr)
{
    uint32_t lo, hi;
    __asm__ volatile("rdmsr" : "=a"(lo), "=d"(hi) : "c"(msr));
    return ((uint64_t)hi << 32) | lo;
}

static inline void wrmsr_raw(uint32_t msr, uint64_t v)
{
    uint32_t lo = (uint32_t)v, hi = (uint32_t)(v >> 32);
    __asm__ volatile("wrmsr" ::"a"(lo), "d"(hi), "c"(msr));
}

static inline uint32_t lapic_read(uint32_t reg)
{
    if (g_x2apic)
        return (uint32_t)rdmsr_raw(X2APIC_MSR_BASE + (reg >> 4));
    if (!g_lapic)
        return 0;
    return *(volatile uint32_t *)((volatile uint8_t *)g_lapic + reg);
}

static inline void lapic_write(uint32_t reg, uint32_t value)
{
    if (g_x2apic)
    {
        wrmsr_raw(X2APIC_MSR_BASE + (reg >> 4), value);
        return;
    }
    if (!g_lapic)
        return;
    *(volatile uint32_t *)((volatile uint8_t *)g_lapic + reg) = value;
}

bool lapic_x2apic_supported(void)
{
    uint32_t a = 1, b, c = 0, d;
    __asm__ volatile("cpuid" : "+a"(a), "=b"(b), "+c"(c), "=d"(d));
    return (c >> 21) & 1;
}

void lapic_set_x2apic(bool enable)
{
    g_x2apic = enable;
}

bool lapic_is_x2apic(void)
{
    return g_x2apic;
}

/* Запись в ICR: в xAPIC два MMIO-write (младшее слово запускает отправку),
 * в x2APIC — один MSR, назначение в битах 63:32. */
static inline void lapic_icr_write(uint32_t dest_apic_id, uint32_t low)
{
    if (g_x2apic)
    {
        wrmsr_raw(X2APIC_MSR_ICR, ((uint64_t)dest_apic_id << 32) | low);
        return;
    }
    lapic_write(LAPIC_REG_ICR_HIGH, (dest_apic_id & 0xFF) << 24);
    lapic_write(LAPIC_REG_ICR_LOW, low);
}

static inline uint64_t rdmsr_apic_base(void)
{
    uint32_t lo, hi;
    __asm__ volatile("rdmsr" : "=a"(lo), "=d"(hi) : "c"(LAPIC_BASE_MSR));
    return ((uint64_t)hi << 32) | lo;
}

static inline void wrmsr_apic_base(uint64_t v)
{
    uint32_t lo = (uint32_t)v, hi = (uint32_t)(v >> 32);
    __asm__ volatile("wrmsr" ::"a"(lo), "d"(hi), "c"(LAPIC_BASE_MSR));
}

/* Задержка по PIT (канал 2, режим 0). Счётчик итераций pause зависит от CPU,
 * а SDM требует точных 10 мс между INIT и SIPI. Канал 0 занят тиком таймера;
 * OUT канала 2 читается из порта 0x61, бит 5. */
#define PIT_HZ 1193182u
#define PIT_PORT_CH2 0x42
#define PIT_PORT_CMD 0x43
#define PIT_PORT_CTRL 0x61

static void pit_delay_ticks(uint16_t ticks)
{
    uint8_t saved = io_read8(PIT_PORT_CTRL);

    io_write8(PIT_PORT_CTRL, (uint8_t)(saved & ~0x03u));
    io_write8(PIT_PORT_CMD, 0xB0);
    io_write8(PIT_PORT_CH2, (uint8_t)(ticks & 0xFF));
    io_write8(PIT_PORT_CH2, (uint8_t)(ticks >> 8));
    io_write8(PIT_PORT_CTRL, (uint8_t)((saved & ~0x02u) | 0x01u));

    /* лимит на случай, если PIT недоступен */
    for (uint32_t spin = 0; spin < 50000000u; spin++)
    {
        if (io_read8(PIT_PORT_CTRL) & 0x20)
            break;
        cpu_relax();
    }

    io_write8(PIT_PORT_CTRL, saved);
}

void lapic_delay_ms(uint32_t ms)
{
    /* 1 мс = 1193 тика */
    for (uint32_t i = 0; i < ms; i++)
        pit_delay_ticks((uint16_t)(PIT_HZ / 1000u));
}

static bool lapic_init_impl(uint64_t base, bool keep_pic)
{
    /* база из MSR точнее, чем из MADT */
    uint64_t msr = rdmsr_apic_base();
    uint64_t phys = msr & 0x000FFFFFFFFFF000ULL;

    if (phys == 0)
        phys = base;
    if (phys == 0)
        phys = 0xFEE00000ULL;

    /* x2APIC включается только поверх xAPIC (иначе #GP) */
    if (!(msr & LAPIC_BASE_EN))
    {
        msr |= LAPIC_BASE_EN;
        wrmsr_apic_base(msr);
    }
    if (g_x2apic && !(msr & LAPIC_BASE_EXTD))
        wrmsr_apic_base(msr | LAPIC_BASE_EN | LAPIC_BASE_EXTD);

    g_lapic_phys = phys;
    g_lapic = (volatile uint32_t *)(uintptr_t)phys; /* identity-mapped */

    lapic_write(LAPIC_REG_SVR, LAPIC_SPURIOUS_VECTOR | LAPIC_SVR_ENABLE);

    lapic_write(LAPIC_REG_TPR, 0);

    /* маскируем ненужные LVT-линии */
    lapic_write(LAPIC_REG_LVT_TIMER, 1u << 16);
    lapic_write(LAPIC_REG_LVT_THERMAL, 1u << 16);
    lapic_write(LAPIC_REG_LVT_PERF, 1u << 16);
    lapic_write(LAPIC_REG_LVT_LINT1, 1u << 16);
    lapic_write(LAPIC_REG_LVT_ERROR, 1u << 16);

    /* пока IRQ идут через 8259, LINT0 должен оставаться в ExtINT */
    if (keep_pic)
        lapic_write(LAPIC_REG_LVT_LINT0, 0x700 | 0x07); /* ExtINT, unmasked */
    else
        lapic_write(LAPIC_REG_LVT_LINT0, 1u << 16);

    lapic_write(LAPIC_REG_ESR, 0);
    lapic_write(LAPIC_REG_ESR, 0);

    return lapic_read(LAPIC_REG_SVR) & LAPIC_SVR_ENABLE;
}

bool lapic_init(uint64_t base)
{
    return lapic_init_impl(base, false);
}

bool lapic_init_extint(uint64_t base, bool keep_pic)
{
    return lapic_init_impl(base, keep_pic);
}

uint32_t lapic_id(void)
{
    uint32_t v = lapic_read(LAPIC_REG_ID);
    return g_x2apic ? v : (v >> 24);
}

void lapic_eoi(void)
{
    lapic_write(LAPIC_REG_EOI, 0);
}

static void lapic_wait_icr_idle(void)
{
    if (g_x2apic)
        return; /* в x2APIC бита Delivery Status нет */
    for (int i = 0; i < 1000000; i++)
    {
        if (!(lapic_read(LAPIC_REG_ICR_LOW) & LAPIC_ICR_DELIVERY_PENDING))
            return;
        cpu_relax();
    }
}

void lapic_send_ipi(uint32_t dest_apic_id, uint32_t vector)
{
    lapic_wait_icr_idle();
    lapic_icr_write(dest_apic_id,
                    LAPIC_ICR_VECTOR(vector) |
                        LAPIC_ICR_DELIVERY_FIXED |
                        LAPIC_ICR_DEST_PHYSICAL |
                        LAPIC_ICR_LEVEL_ASSERT);
    lapic_wait_icr_idle();
}

void lapic_send_ipi_all_but_self(uint32_t vector)
{
    lapic_wait_icr_idle();
    lapic_icr_write(0,
                    LAPIC_ICR_VECTOR(vector) |
                        LAPIC_ICR_DELIVERY_FIXED |
                        LAPIC_ICR_LEVEL_ASSERT |
                        LAPIC_ICR_DEST_ALL_BUT_SELF);
    lapic_wait_icr_idle();
}

void lapic_send_init(uint32_t dest_apic_id)
{
    lapic_wait_icr_idle();
    lapic_icr_write(dest_apic_id,
                    LAPIC_ICR_DELIVERY_INIT |
                        LAPIC_ICR_DEST_PHYSICAL |
                        LAPIC_ICR_LEVEL_ASSERT |
                        LAPIC_ICR_TRIGGER_LEVEL);
    lapic_wait_icr_idle();
}

void lapic_send_init_all_but_self(void)
{
    lapic_wait_icr_idle();
    lapic_icr_write(0,
                    LAPIC_ICR_DELIVERY_INIT |
                        LAPIC_ICR_LEVEL_ASSERT |
                        LAPIC_ICR_TRIGGER_LEVEL |
                        LAPIC_ICR_DEST_ALL_BUT_SELF);
    lapic_wait_icr_idle();
}

/* INIT level de-assert (SDM 8.4.4.1): нужен старым ядрам, в x2APIC его нет */
void lapic_send_init_deassert_all_but_self(void)
{
    if (g_x2apic)
        return;
    lapic_wait_icr_idle();
    lapic_icr_write(0,
                    LAPIC_ICR_DELIVERY_INIT |
                        LAPIC_ICR_TRIGGER_LEVEL | /* level=0 -> de-assert */
                        LAPIC_ICR_DEST_ALL_BUT_SELF);
    lapic_wait_icr_idle();
}

void lapic_send_sipi(uint32_t dest_apic_id, uint8_t vector_page)
{
    lapic_wait_icr_idle();
    lapic_icr_write(dest_apic_id,
                    LAPIC_ICR_VECTOR(vector_page) |
                        LAPIC_ICR_DELIVERY_SIPI |
                        LAPIC_ICR_DEST_PHYSICAL |
                        LAPIC_ICR_LEVEL_ASSERT);
    lapic_wait_icr_idle();
}
