// power/poweroff.c
#include "poweroff.h"
#include "../portio/portio.h"
#include <stdint.h>
#include <stddef.h>

#ifndef phys_to_virt
#define phys_to_virt(p) ((void *)(uintptr_t)(p))
#endif

static inline void *p2v(uintptr_t p) { return phys_to_virt(p); }

#define SIG_RSDP "RSD PTR "
#define SIG_FACP "FACP"
#define PM1_SLP_EN (1 << 13)

#pragma pack(push, 1)
typedef struct
{
    char signature[8];
    uint8_t checksum;
    char oemid[6];
    uint8_t revision;
    uint32_t rsdt_address;
    uint32_t length;
    uint64_t xsdt_address;
    uint8_t ext_checksum;
    uint8_t reserved[3];
} rsdp2_t;

typedef struct
{
    char sig[4];
    uint32_t length;
    uint8_t revision;
    uint8_t checksum;
    char oemid[6];
    char oemtableid[8];
    uint32_t oemrev;
    uint32_t creatorid;
    uint32_t creatorrev;
} acpi_sdt_hdr_t;

typedef struct
{
    uint8_t address_space;
    uint8_t bit_width;
    uint8_t bit_offset;
    uint8_t access_size;
    uint64_t address;
} acpi_gas_t;
#pragma pack(pop)

static int memcmp_simple(const void *a, const void *b, size_t n)
{
    const unsigned char *pa = a, *pb = b;
    for (size_t i = 0; i < n; ++i)
        if (pa[i] != pb[i])
            return (int)pa[i] - (int)pb[i];
    return 0;
}

static int checksum_ok(const void *p, size_t n)
{
    const uint8_t *q = (const uint8_t *)p;
    uint8_t s = 0;
    for (size_t i = 0; i < n; ++i)
        s = (uint8_t)(s + q[i]);
    return s == 0;
}

static uint32_t read_u32(const void *p)
{
    uint32_t v;
    __builtin_memcpy(&v, p, sizeof(v));
    return v;
}
static uint64_t read_u64(const void *p)
{
    uint64_t v;
    __builtin_memcpy(&v, p, sizeof(v));
    return v;
}

static uintptr_t find_rsdp(void)
{
    for (uintptr_t a = 0x000E0000; a <= 0x000FFFFF; a += 16)
    {
        void *vp = p2v(a);
        if (!vp)
            continue;
        if (memcmp_simple(vp, SIG_RSDP, 8) == 0)
        {
            rsdp2_t *r = (rsdp2_t *)vp;
            if ((r->revision >= 2 && r->length && checksum_ok(r, r->length)) || checksum_ok(r, 20))
                return a;
        }
    }
    return 0;
}

static acpi_sdt_hdr_t *map_sdt(uintptr_t phys)
{
    void *vp = p2v(phys);
    if (!vp)
        return NULL;
    acpi_sdt_hdr_t *h = (acpi_sdt_hdr_t *)vp;
    if (h->length < sizeof(acpi_sdt_hdr_t) || h->length > 0x200000)
        return NULL;
    if (!checksum_ok(h, h->length))
        return NULL;
    return h;
}

static uintptr_t find_table(uintptr_t table_phys, const char *sig, int xsdt)
{
    acpi_sdt_hdr_t *t = map_sdt(table_phys);
    if (!t)
        return 0;

    uint8_t *base = (uint8_t *)p2v(table_phys) + sizeof(acpi_sdt_hdr_t);
    if (!base)
        return 0;

    if (!xsdt)
    {
        int entries = (t->length - sizeof(acpi_sdt_hdr_t)) / 4;
        for (int i = 0; i < entries; i++)
        {
            uint32_t e = read_u32(base + i * 4);
            acpi_sdt_hdr_t *h = map_sdt((uintptr_t)e);
            if (h && memcmp_simple(h->sig, sig, 4) == 0)
                return (uintptr_t)e;
        }
    }
    else
    {
        int entries = (t->length - sizeof(acpi_sdt_hdr_t)) / 8;
        for (int i = 0; i < entries; i++)
        {
            uint64_t e = read_u64(base + i * 8);
            acpi_sdt_hdr_t *h = map_sdt((uintptr_t)e);
            if (h && memcmp_simple(h->sig, sig, 4) == 0)
                return (uintptr_t)e;
        }
    }
    return 0;
}

static int find_s5(uintptr_t dsdt_phys, uint8_t *slp_a, uint8_t *slp_b)
{
    acpi_sdt_hdr_t *dsdt = map_sdt(dsdt_phys);
    if (!dsdt)
        return -1;

    uint8_t *p = (uint8_t *)p2v(dsdt_phys);
    size_t len = dsdt->length;

    for (size_t i = 0; i + 4 < len; ++i)
    {
        if (memcmp_simple(&p[i], "_S5_", 4) == 0)
        {
            size_t pos = i + 4;
            if (pos + 6 < len)
            {
                *slp_a = p[pos + 5];
                *slp_b = p[pos + 7];
                return 0;
            }
        }
    }
    return -1;
}

static void io_wait(void) { outb(0x80, 0); }

static void write_pm(uint64_t addr, int is_io, uint16_t value)
{
    if (!addr)
        return;
    if (is_io)
    {
        if (addr <= 0xFFFF)
            outw((uint16_t)addr, value);
        else
        {
            volatile uint16_t *mm = (volatile uint16_t *)p2v(addr);
            if (mm)
                *mm = value;
        }
    }
    else
    {
        volatile uint16_t *mm = (volatile uint16_t *)p2v(addr);
        if (mm)
            *mm = value;
    }
}

static void emulator_fallback(void)
{
    outw(0x604, 0x2000);
    io_wait();
    outw(0x4004, 0x2000);
    io_wait();
}

static int try_find_gas_in_fadt(uint8_t *fadt_base, uint32_t fadt_len,
                                uint64_t *pm1a_addr, uint64_t *pm1b_addr, int *pm1_io)
{
    const int candidate_offsets[][2] = {
        {76, 84},
        {80, 88},
        {92, 100},
        {100, 108},
        {116, 124},
        {128, 136},
        {140, 148},
    };
    int candidates = sizeof(candidate_offsets) / sizeof(candidate_offsets[0]);

    for (int c = 0; c < candidates; ++c)
    {
        int off_a = candidate_offsets[c][0];
        int off_b = candidate_offsets[c][1];
        if ((size_t)off_a + sizeof(acpi_gas_t) <= fadt_len)
        {
            acpi_gas_t gas = {0};
            __builtin_memcpy(&gas, fadt_base + off_a, sizeof(acpi_gas_t));
            if (gas.address)
            {
                *pm1a_addr = gas.address;
                *pm1_io = (gas.address_space == 0);
            }
        }
        if ((size_t)off_b + sizeof(acpi_gas_t) <= fadt_len)
        {
            acpi_gas_t gas = {0};
            __builtin_memcpy(&gas, fadt_base + off_b, sizeof(acpi_gas_t));
            if (gas.address)
            {
                *pm1b_addr = gas.address;
                if (!(*pm1_io) && gas.address_space == 0)
                    *pm1_io = 1;
            }
        }
        if (*pm1a_addr || *pm1b_addr)
            return 0;
    }
    return -1;
}

void power_off(void)
{
    emulator_fallback();

    uintptr_t rsdp_phys = find_rsdp();
    if (!rsdp_phys)
    {
        emulator_fallback();
        for (;;)
            asm volatile("hlt");
    }

    rsdp2_t *rsdp = (rsdp2_t *)p2v(rsdp_phys);
    if (!rsdp)
    {
        emulator_fallback();
        for (;;)
            asm volatile("hlt");
    }

    int use_xsdt = (rsdp->revision >= 2 && rsdp->xsdt_address != 0);
    uintptr_t table_phys = use_xsdt ? (uintptr_t)rsdp->xsdt_address : (uintptr_t)rsdp->rsdt_address;
    if (!table_phys)
    {
        emulator_fallback();
        for (;;)
            asm volatile("hlt");
    }

    uintptr_t fadt_phys = find_table(table_phys, SIG_FACP, use_xsdt);
    if (!fadt_phys)
    {
        emulator_fallback();
        for (;;)
            asm volatile("hlt");
    }

    acpi_sdt_hdr_t *fadt_hdr = map_sdt(fadt_phys);
    if (!fadt_hdr)
    {
        emulator_fallback();
        for (;;)
            asm volatile("hlt");
    }

    uint8_t *fadt_base = (uint8_t *)p2v(fadt_phys);
    uint32_t fadt_len = fadt_hdr->length;

    uint64_t dsdt_addr = 0;
    if (fadt_len >= 140)
    {
        dsdt_addr = read_u64(fadt_base + 140);
    }
    if (!dsdt_addr)
    {
        if (fadt_len >= 44)
        {
            uint32_t d32 = read_u32(fadt_base + 40);
            dsdt_addr = (uint64_t)d32;
        }
    }

    uint64_t pm1a_addr = 0, pm1b_addr = 0;
    int pm1_io = 1;

    if (fadt_len >= 72)
    {
        uint32_t p1 = read_u32(fadt_base + 64);
        uint32_t p2 = read_u32(fadt_base + 68);
        if (p1)
            pm1a_addr = p1;
        if (p2)
            pm1b_addr = p2;
    }

    if (!pm1a_addr && !pm1b_addr)
    {
        try_find_gas_in_fadt(fadt_base, fadt_len, &pm1a_addr, &pm1b_addr, &pm1_io);
    }

    if (!pm1a_addr && !pm1b_addr)
    {
        size_t payload_off = sizeof(acpi_sdt_hdr_t);
        for (size_t i = payload_off; i + sizeof(acpi_gas_t) <= fadt_len; i += 4)
        {
            acpi_gas_t gas;
            __builtin_memcpy(&gas, fadt_base + i, sizeof(acpi_gas_t));
            if (gas.address && (gas.address_space == 0 || gas.address_space == 1) &&
                gas.bit_width > 0 && gas.bit_width <= 64)
            {
                if (!pm1a_addr)
                {
                    pm1a_addr = gas.address;
                    pm1_io = (gas.address_space == 0);
                }
                else if (!pm1b_addr)
                {
                    pm1b_addr = gas.address;
                    if (!pm1_io && gas.address_space == 0)
                        pm1_io = 1;
                    break;
                }
            }
        }
    }

    uint8_t s5a = 5, s5b = 0;
    if (dsdt_addr && find_s5((uintptr_t)dsdt_addr, &s5a, &s5b) != 0)
    {
    }

    uint16_t sleep_value = (uint16_t)((s5a & 0x7) << 10) | PM1_SLP_EN;

    if (pm1a_addr)
        write_pm(pm1a_addr, pm1_io, sleep_value);
    if (pm1b_addr)
        write_pm(pm1b_addr, pm1_io, sleep_value);

    io_wait();

    emulator_fallback();

    for (;;)
        asm volatile("hlt");
}
