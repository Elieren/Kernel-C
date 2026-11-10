/* acpi.c — минимальный парсер ACPI: RSDP -> RSDT/XSDT -> MADT.
 * Из MADT берутся APIC ID ядер и адрес MMIO Local APIC.
 * Адреса identity-mapped: физический == виртуальный. */
#include <asm/acpi.h>
#include "drivers/serial/serial.h"
#include "lib/string/string.h"
#include "lib/graphics/formatting/formatting.h"

#define RSDP_SIG "RSD PTR "
#define RSDP_V1_LEN 20
#define RSDP_V2_LEN 36

#define ACPI_HEADER_LEN 36

#define MADT_TYPE_LAPIC 0
#define MADT_TYPE_IOAPIC 1
#define MADT_TYPE_LAPIC_OVERRIDE 5
#define MADT_TYPE_LAPIC_X2APIC 9

static acpi_info_t g_acpi;
static uint64_t g_rsdp = 0;

/* чтение по возможно невыровненным адресам */
static inline uint8_t rd8(uint64_t a) { return *(volatile uint8_t *)(uintptr_t)a; }
static inline uint16_t rd16(uint64_t a)
{
    uint16_t v;
    memcpy(&v, (const void *)(uintptr_t)a, sizeof(v));
    return v;
}
static inline uint32_t rd32(uint64_t a)
{
    uint32_t v;
    memcpy(&v, (const void *)(uintptr_t)a, sizeof(v));
    return v;
}
static inline uint64_t rd64(uint64_t a)
{
    uint64_t v;
    memcpy(&v, (const void *)(uintptr_t)a, sizeof(v));
    return v;
}

static int checksum_ok(uint64_t addr, uint32_t len)
{
    uint8_t sum = 0;
    for (uint32_t i = 0; i < len; i++)
        sum += rd8(addr + i);
    return sum == 0;
}

static int rsdp_valid(uint64_t rsdp)
{
    if (!rsdp)
        return 0;
    if (memcmp((const void *)(uintptr_t)rsdp, RSDP_SIG, 8) != 0)
        return 0;
    if (!checksum_ok(rsdp, RSDP_V1_LEN))
        return 0;

    uint8_t rev = rd8(rsdp + 15);
    if (rev >= 2)
    {
        uint32_t len = rd32(rsdp + 20);
        if (len >= RSDP_V2_LEN && len <= 4096 && !checksum_ok(rsdp, len))
            return 0;
    }
    return 1;
}

static uint64_t scan_root_table(uint64_t root, int use_xsdt, const char *sig)
{
    if (!root)
        return 0;
    if (memcmp((const void *)(uintptr_t)root, use_xsdt ? "XSDT" : "RSDT", 4) != 0)
        return 0;

    uint32_t len = rd32(root + 4);
    if (len < ACPI_HEADER_LEN || len > (1u << 20))
        return 0;

    uint32_t entries = (len - ACPI_HEADER_LEN) / (use_xsdt ? 8 : 4);
    for (uint32_t i = 0; i < entries; i++)
    {
        uint64_t entry_addr = root + ACPI_HEADER_LEN + (uint64_t)i * (use_xsdt ? 8 : 4);
        uint64_t tbl = use_xsdt ? rd64(entry_addr) : (uint64_t)rd32(entry_addr);
        if (!tbl)
            continue;
        if (memcmp((const void *)(uintptr_t)tbl, sig, 4) == 0)
            return tbl;
    }
    return 0;
}

uint64_t acpi_find_table(const char *signature)
{
    if (!rsdp_valid(g_rsdp))
        return 0;

    uint8_t rev = rd8(g_rsdp + 15);
    if (rev >= 2)
    {
        uint64_t t = scan_root_table(rd64(g_rsdp + 24), 1, signature);
        if (t)
            return t;
    }
    return scan_root_table((uint64_t)rd32(g_rsdp + 16), 0, signature);
}

static void add_cpu(uint32_t id, uint8_t flags)
{
    if (g_acpi.cpu_count >= ACPI_MAX_CPUS)
        return;

    /* в некоторых прошивках бывают дубликаты */
    for (uint32_t i = 0; i < g_acpi.cpu_count; i++)
        if (g_acpi.cpus[i].apic_id == id)
            return;

    acpi_cpu_t *c = &g_acpi.cpus[g_acpi.cpu_count++];
    c->apic_id = id;
    c->processor_id = id;
    c->enabled = (flags & 0x1) != 0;
    c->online = false;
    c->is_bsp = false;
    if (c->enabled)
        g_acpi.enabled_count++;
}

static void parse_madt(uint64_t madt)
{
    if (!madt)
        return;
    if (memcmp((const void *)(uintptr_t)madt, "APIC", 4) != 0)
        return;

    uint32_t len = rd32(madt + 4);
    if (len < ACPI_HEADER_LEN + 8 || len > (1u << 20))
        return;

    g_acpi.madt_found = true;
    g_acpi.madt_addr = madt;
    g_acpi.lapic_base = (uint64_t)rd32(madt + ACPI_HEADER_LEN); /* 32-бит адрес LAPIC */
    g_acpi.madt_flags = rd8(madt + ACPI_HEADER_LEN + 4);

    uint64_t p = madt + ACPI_HEADER_LEN + 8;
    uint64_t end = madt + len;

    while (p + 2 <= end)
    {
        uint8_t type = rd8(p);
        uint8_t elen = rd8(p + 1);
        if (elen < 2 || p + elen > end)
            break;

        if (type == MADT_TYPE_LAPIC && elen >= 8)
        {
            add_cpu(rd8(p + 3), rd8(p + 4));
        }
        else if (type == MADT_TYPE_LAPIC_X2APIC && elen >= 16)
        {
            /* x2APIC: полный 32-битный id */
            add_cpu(rd32(p + 4), rd8(p + 8));
        }
        else if (type == MADT_TYPE_LAPIC_OVERRIDE && elen >= 12)
        {
            g_acpi.lapic_base = rd64(p + 4);
        }

        p += elen;
    }
}

bool acpi_init(uint64_t rsdp_phys)
{
    memset(&g_acpi, 0, sizeof(g_acpi));
    g_rsdp = rsdp_phys;

    /* ACPI-тега от Multiboot2 нет — ищем RSDP в EBDA и в BIOS ROM */
    if (!rsdp_valid(g_rsdp))
    {
        /* сегмент EBDA лежит по 0x40E */
        uint16_t ebda_seg = 0;
        __asm__ volatile("movw 0x40E, %0" : "=r"(ebda_seg));
        uint64_t ebda = (uint64_t)ebda_seg << 4;
        if (ebda >= 0x400 && ebda < 0xA0000)
        {
            for (uint64_t p = ebda; p < ebda + 1024; p += 16)
                if (rsdp_valid(p))
                {
                    g_rsdp = p;
                    break;
                }
        }
    }

    if (!rsdp_valid(g_rsdp))
    {
        for (uint64_t p = 0xE0000; p < 0x100000; p += 16)
            if (rsdp_valid(p))
            {
                g_rsdp = p;
                break;
            }
    }

    if (!rsdp_valid(g_rsdp))
        return false;

    uint64_t madt = acpi_find_table("APIC");
    if (!madt)
        return false;

    parse_madt(madt);
    return g_acpi.madt_found;
}

const acpi_info_t *acpi_get_info(void)
{
    return &g_acpi;
}

void acpi_dump_to_serial(void)
{
    char buf[112];
    const acpi_info_t *a = &g_acpi;

    serial_write_string(1, "[acpi] MADT: ");
    serial_write_string(1, a->madt_found ? "found" : "missing");
    serial_write_string(1, "\n");

    kformat(buf, sizeof(buf), "[acpi] lapic_base=0x%llx cpus=%u enabled=%u\n",
            (unsigned long long)a->lapic_base,
            (unsigned)a->cpu_count, (unsigned)a->enabled_count);
    serial_write_string(1, buf);

    /* serial медленный: печатаем первые 16 и последнее */
    for (uint32_t i = 0; i < a->cpu_count; i++)
    {
        if (a->cpu_count > 20 && i >= 16 && i != a->cpu_count - 1)
        {
            if (i == 16)
                serial_write_string(1, "[acpi]   ...\n");
            continue;
        }
        kformat(buf, sizeof(buf), "[acpi]   cpu%u apic_id=%u %s\n",
                (unsigned)i, (unsigned)a->cpus[i].apic_id,
                a->cpus[i].enabled ? "enabled" : "disabled");
        serial_write_string(1, buf);
    }
}
