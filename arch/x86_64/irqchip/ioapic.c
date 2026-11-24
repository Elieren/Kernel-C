/* ioapic.c — минимальный драйвер I/O APIC (выбор ядра-получателя для IRQ). */
#include <asm/ioapic.h>
#include <asm/acpi.h>
#include "mm/paging/paging.h"

#define IOAPIC_REGSEL 0x00
#define IOAPIC_WIN 0x10

#define IOAPIC_REG_VER 0x01
#define IOAPIC_REG_RTE(n) (0x10u + 2u * (n)) /* младший dword; старший = +1 */

#define RTE_MASKED (1u << 16)
#define RTE_LEVEL (1u << 15)
#define RTE_ACTIVE_LOW (1u << 13)

typedef struct
{
    volatile uint8_t *base;
    uint32_t gsi_base;
    uint32_t entries; /* число входов (max redirection entry + 1) */
} ioapic_t;

static ioapic_t g_io[ACPI_MAX_IOAPICS];
static uint32_t g_io_count;

/* два MMIO-доступа на операцию: нужна блокировка */
static volatile int g_io_lock;

static unsigned long io_lock(void)
{
    unsigned long flags;
    __asm__ volatile("pushfq; popq %0; cli" : "=r"(flags)::"memory");
    while (__atomic_exchange_n(&g_io_lock, 1, __ATOMIC_ACQUIRE))
        __asm__ volatile("pause");
    return flags;
}

static void io_unlock(unsigned long flags)
{
    __atomic_store_n(&g_io_lock, 0, __ATOMIC_RELEASE);
    if (flags & (1ul << 9))
        __asm__ volatile("sti" ::: "memory");
}

static uint32_t io_read(const ioapic_t *io, uint32_t reg)
{
    *(volatile uint32_t *)(io->base + IOAPIC_REGSEL) = reg;
    return *(volatile uint32_t *)(io->base + IOAPIC_WIN);
}

static void io_write(const ioapic_t *io, uint32_t reg, uint32_t val)
{
    *(volatile uint32_t *)(io->base + IOAPIC_REGSEL) = reg;
    *(volatile uint32_t *)(io->base + IOAPIC_WIN) = val;
}

static ioapic_t *find_io(uint32_t gsi, uint32_t *pin)
{
    for (uint32_t i = 0; i < g_io_count; i++)
    {
        ioapic_t *io = &g_io[i];
        if (gsi >= io->gsi_base && gsi < io->gsi_base + io->entries)
        {
            *pin = gsi - io->gsi_base;
            return io;
        }
    }
    return 0;
}

bool ioapic_init(void)
{
    const acpi_info_t *a = acpi_get_info();
    g_io_count = 0;

    if (!a || !a->madt_found)
        return false;

    for (uint32_t i = 0; i < a->ioapic_count; i++)
    {
        uint64_t addr = a->ioapics[i].addr;
        if (addr == 0)
            continue;

        ioapic_t *io = &g_io[g_io_count];
        io->base = (volatile uint8_t *)(uintptr_t)addr; /* identity-mapped */
        io->gsi_base = a->ioapics[i].gsi_base;

        uint32_t ver = io_read(io, IOAPIC_REG_VER);
        if (ver == 0xFFFFFFFFu) /* нет устройства */
            continue;
        io->entries = ((ver >> 16) & 0xFFu) + 1u;

        /* доступ под CR3 user-задачи */
        paging_reserve_kernel_mmio(addr, 4096);
        g_io_count++;
    }

    return g_io_count != 0;
}

bool ioapic_present(void)
{
    return g_io_count != 0;
}

bool ioapic_resolve_isa_irq(uint8_t isa_irq, uint32_t *gsi, bool *active_low, bool *level)
{
    if (!g_io_count)
        return false;

    /* По умолчанию ISA: GSI = IRQ, active high, edge */
    uint32_t g = isa_irq;
    bool low = false, lvl = false;

    const acpi_info_t *a = acpi_get_info();
    for (uint32_t i = 0; a && i < a->iso_count; i++)
    {
        const acpi_iso_t *o = &a->isos[i];
        if (o->bus != 0 || o->src_irq != isa_irq)
            continue;
        g = o->gsi;
        uint32_t pol = o->flags & 3u;        /* 1 = high, 3 = low */
        uint32_t trg = (o->flags >> 2) & 3u; /* 1 = edge, 3 = level */
        low = (pol == 3u);
        lvl = (trg == 3u);
        break;
    }

    uint32_t pin;
    if (!find_io(g, &pin))
        return false;

    *gsi = g;
    *active_low = low;
    *level = lvl;
    return true;
}

bool ioapic_setup_route(uint32_t gsi, uint8_t vector, uint32_t dest_apic_id,
                        bool active_low, bool level)
{
    uint32_t pin;
    ioapic_t *io = find_io(gsi, &pin);
    if (!io || dest_apic_id > IOAPIC_MAX_DEST_ID)
        return false;

    uint32_t low = vector | RTE_MASKED | (active_low ? RTE_ACTIVE_LOW : 0) | (level ? RTE_LEVEL : 0);

    unsigned long f = io_lock();
    /* сначала назначение при замаскированном входе, потом младший dword */
    io_write(io, IOAPIC_REG_RTE(pin) + 1, dest_apic_id << 24);
    io_write(io, IOAPIC_REG_RTE(pin), low);
    io_unlock(f);
    return true;
}

bool ioapic_set_dest(uint32_t gsi, uint32_t dest_apic_id)
{
    uint32_t pin;
    ioapic_t *io = find_io(gsi, &pin);
    if (!io || dest_apic_id > IOAPIC_MAX_DEST_ID)
        return false;

    unsigned long f = io_lock();
    io_write(io, IOAPIC_REG_RTE(pin) + 1, dest_apic_id << 24);
    io_unlock(f);
    return true;
}

static void set_mask(uint32_t gsi, bool mask)
{
    uint32_t pin;
    ioapic_t *io = find_io(gsi, &pin);
    if (!io)
        return;

    unsigned long f = io_lock();
    uint32_t low = io_read(io, IOAPIC_REG_RTE(pin));
    low = mask ? (low | RTE_MASKED) : (low & ~RTE_MASKED);
    io_write(io, IOAPIC_REG_RTE(pin), low);
    io_unlock(f);
}

void ioapic_mask(uint32_t gsi)
{
    set_mask(gsi, true);
}

void ioapic_unmask(uint32_t gsi)
{
    set_mask(gsi, false);
}
