// power/poweroff.c
#include "../portio/portio.h"
#include <stdint.h>
#include <stddef.h>

/*
 * Assumptions:
 *  - kernel has identity map for physical memory (cast phys->ptr is OK)
 *
 * Usage:
 *  - call acpi_init(); it returns 0 on success (found FADT+_S5_)
 *  - call acpi_poweroff(); it will try to enable ACPI (if needed)
 *    and write PM1x_CNT with (SLP_TYP << 10) | SLP_EN.
 */

/* RSDP (revision 1) layout first 20 bytes */
struct rsdp1
{
    char sig[8]; /* "RSD PTR " */
    uint8_t checksum;
    char oemid[6];
    uint8_t revision;
    uint32_t rsdt_address;
} __attribute__((packed));

/* Generic ACPI header present at start of RSDT/FADT/DSDT */
struct acpi_header
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
} __attribute__((packed));

/* We'll index into FADT by known offsets per ACPI 1.0/2.0 (32-bit fields) */
#define FADT_DSDT_OFFSET 40         /* offset of 32-bit DSDT pointer in FADT */
#define FADT_SMI_CMD_OFFSET 48      /* SMI_CMD port */
#define FADT_ACPI_ENABLE_OFFSET 52  /* ACPI_ENABLE (byte) */
#define FADT_ACPI_DISABLE_OFFSET 53 /* ACPI_DISABLE (byte) */
#define FADT_PM1A_CNT_BLK_OFFSET 64 /* PM1a_CNT_BLK (32-bit) */
#define FADT_PM1B_CNT_BLK_OFFSET 68 /* PM1b_CNT_BLK (32-bit) */
#define FADT_PM1_CNT_LEN_OFFSET 89  /* PM1_CNT_LEN (byte) - typical offset, safe to read if length large */

/* Globals discovered */
static uint32_t g_pm1a_cnt = 0;
static uint32_t g_pm1b_cnt = 0;
static uint16_t g_slp_typa = 0;
static uint16_t g_slp_typb = 0;
static uint16_t g_slp_en = 1U << 13; /* SLP_EN is bit 13 */
static uint32_t g_smi_cmd = 0;
static uint8_t g_acpi_enable = 0;
static uint8_t g_acpi_disable = 0;
static int g_acpi_valid = 0;

/* helper: compute checksum of table at ptr of length bytes */
static int acpi_checksum(void *ptr, size_t len)
{
    uint8_t *b = (uint8_t *)ptr;
    uint8_t sum = 0;
    for (size_t i = 0; i < len; ++i)
        sum += b[i];
    return sum == 0;
}

/* find RSDP: search EBDA and 0xE0000..0xFFFFF as usual */
static struct rsdp1 *find_rsdp(void)
{
    /* 1) search 0x000E0000 .. 0x000FFFFF aligned by 16 */
    for (uintptr_t p = 0x000E0000; p < 0x00100000; p += 16)
    {
        struct rsdp1 *r = (struct rsdp1 *)p;
        if (r->sig[0] == 'R' && r->sig[1] == 'S' && r->sig[2] == 'D' && r->sig[3] == ' ' &&
            r->sig[4] == 'P' && r->sig[5] == 'T' && r->sig[6] == 'R' && r->sig[7] == ' ')
        {
            if (acpi_checksum(r, sizeof(struct rsdp1)))
                return r;
        }
    }
    /* 2) EBDA: at 0x40E (word) is segment*16 */
    uint16_t ebda_seg = *(uint16_t *)0x40E;
    if (ebda_seg)
    {
        uintptr_t ebda = (uintptr_t)ebda_seg << 4;
        for (uintptr_t p = ebda; p < ebda + 1024; p += 16)
        {
            struct rsdp1 *r = (struct rsdp1 *)p;
            if (r->sig[0] == 'R' && r->sig[1] == 'S' && r->sig[2] == 'D' && r->sig[3] == ' ' &&
                r->sig[4] == 'P' && r->sig[5] == 'T' && r->sig[6] == 'R' && r->sig[7] == ' ')
            {
                if (acpi_checksum(r, sizeof(struct rsdp1)))
                    return r;
            }
        }
    }
    return NULL;
}

/* Read RSDT (32-bit) and find FADT table pointer */
static uint32_t find_fadt_from_rsdt(uint32_t rsdt_phys)
{
    struct acpi_header *rsdt = (struct acpi_header *)(uintptr_t)rsdt_phys;
    if (!acpi_checksum(rsdt, rsdt->length))
        return 0;
    /* entries start at offset 36 (header) */
    int entries = (rsdt->length - sizeof(struct acpi_header)) / 4;
    uint32_t *entry = (uint32_t *)((uintptr_t)rsdt + sizeof(struct acpi_header));
    for (int i = 0; i < entries; ++i)
    {
        struct acpi_header *hdr = (struct acpi_header *)(uintptr_t)entry[i];
        if (hdr && hdr->sig[0] == 'F' && hdr->sig[1] == 'A' && hdr->sig[2] == 'C' && hdr->sig[3] == 'P')
        {
            /* verify checksum */
            if (acpi_checksum(hdr, hdr->length))
                return entry[i];
        }
    }
    return 0;
}

/* parse package length per AML encoding (returns pkglen and advances pointer via idx) */
static int aml_pkg_length(uint8_t *buf, size_t buf_len, size_t *idx, size_t *out_len)
{
    if (*idx >= buf_len)
        return -1;
    uint8_t byte0 = buf[*idx];
    (*idx)++;
    uint32_t pkglen = byte0 & 0x0F;
    int byte_count = (byte0 >> 6) & 0x03;
    for (int i = 0; i < byte_count; ++i)
    {
        if (*idx >= buf_len)
            return -1;
        pkglen |= ((uint32_t)buf[*idx]) << (4 + 8 * i);
        (*idx)++;
    }
    *out_len = pkglen;
    return 0;
}

/* scan DSDT for "_S5_" and extract SLP_TYPa/SLP_TYPb (returns 0 on success) */
static int parse_s5_from_dsdt(uint32_t dsdt_phys)
{
    struct acpi_header *dsdt = (struct acpi_header *)(uintptr_t)dsdt_phys;
    if (!acpi_checksum(dsdt, dsdt->length))
        return -1;
    uint8_t *start = (uint8_t *)((uintptr_t)dsdt + sizeof(struct acpi_header));
    size_t len = dsdt->length - sizeof(struct acpi_header);

    for (size_t i = 0; i + 4 <= len; ++i)
    {
        /* find ASCII "_S5_" */
        if (start[i] == '_' && start[i + 1] == 'S' && start[i + 2] == '5' && start[i + 3] == '_')
        {
            /* check NameOp preceding or backslash */
            size_t namepos = i;
            if (namepos >= 1 && start[namepos - 1] == 0x5A)
            {
                /* good (NameOp 0x08/0x5A?), continue */
            }
            else if (namepos >= 2 && start[namepos - 2] == '\\' && start[namepos - 1] == 0x5A)
            {
                /* fine */
            }
            else
            {
                /* not standard, but still try */
            }
            size_t p = i + 4;
            if (p >= len)
                continue;
            /* expect PackageOp (0x12) next (sometimes after other bytes) */
            if (start[p] != 0x12)
            {
                /* not package; continue searching */
                continue;
            }
            p++; /* now at pkgLength */
            size_t pkg_index = p;
            size_t pkglen;
            if (aml_pkg_length(start, len, &pkg_index, &pkglen) < 0)
                continue;

            /* after pkglen, there is NumElements (1 byte) */
            if (pkg_index >= len)
                continue;
            uint8_t num_elems = start[pkg_index++];
            /* now elements follow: we need the first two numeric elements
               elements may be prefixed by BytePrefix (0x0A) then the byte value */
            int found = 0;
            uint8_t val_a = 0, val_b = 0;
            for (int el = 0; el < num_elems && pkg_index < len; ++el)
            {
                uint8_t prefix = start[pkg_index++];
                if (prefix == 0x0A)
                {
                    /* byte prefix */
                    if (pkg_index >= len)
                        break;
                    uint8_t val = start[pkg_index++];
                    if (found == 0)
                    {
                        val_a = val;
                        found++;
                    }
                    else if (found == 1)
                    {
                        val_b = val;
                        found++;
                        break;
                    }
                }
                else if (prefix == 0x0C)
                {
                    /* word prefix (2 bytes) */
                    if (pkg_index + 1 >= len)
                        break;
                    uint16_t v = start[pkg_index] | (start[pkg_index + 1] << 8);
                    pkg_index += 2;
                    if (found == 0)
                    {
                        val_a = v & 0xFF;
                        found++;
                    }
                    else if (found == 1)
                    {
                        val_b = v & 0xFF;
                        found++;
                        break;
                    }
                }
                else if (prefix == 0x0B)
                {
                    /* dword prefix (4 bytes) */
                    if (pkg_index + 3 >= len)
                        break;
                    uint32_t v = start[pkg_index] | (start[pkg_index + 1] << 8) | (start[pkg_index + 2] << 16) | (start[pkg_index + 3] << 24);
                    pkg_index += 4;
                    if (found == 0)
                    {
                        val_a = v & 0xFF;
                        found++;
                    }
                    else if (found == 1)
                    {
                        val_b = v & 0xFF;
                        found++;
                        break;
                    }
                }
                else
                {
                    if (prefix < 0x40)
                    {
                        if (found == 0)
                        {
                            val_a = prefix;
                            found++;
                        }
                        else if (found == 1)
                        {
                            val_b = prefix;
                            found++;
                            break;
                        }
                    }
                    else
                    {
                        /* unknown: bail out */
                        break;
                    }
                }
            }
            if (found >= 1)
            {
                g_slp_typa = (uint16_t)val_a;
                g_slp_typb = (uint16_t)val_b;
                return 0;
            }
        }
    }
    return -1;
}

/* top-level init: find RSDP -> RSDT -> FADT -> DSDT -> parse _S5_ */
int acpi_init(void)
{
    struct rsdp1 *rsdp = find_rsdp();
    if (!rsdp)
        return -1;
    uint32_t rsdt = rsdp->rsdt_address;
    if (!rsdt)
        return -1;
    uint32_t fadt = find_fadt_from_rsdt(rsdt);
    if (!fadt)
        return -1;

    /* read fields from FADT by offsets (ensure within length) */
    struct acpi_header *fh = (struct acpi_header *)(uintptr_t)fadt;
    if (!acpi_checksum(fh, fh->length))
        return -1;

    /* DSDT pointer is a 32-bit field at offset FADT_DSDT_OFFSET */
    uint32_t dsdt = *(uint32_t *)((uintptr_t)fh + FADT_DSDT_OFFSET);
    if (!dsdt)
        return -1;

    /* SMI_CMD */
    g_smi_cmd = *(uint32_t *)((uintptr_t)fh + FADT_SMI_CMD_OFFSET);
    g_acpi_enable = *(uint8_t *)((uintptr_t)fh + FADT_ACPI_ENABLE_OFFSET);
    g_acpi_disable = *(uint8_t *)((uintptr_t)fh + FADT_ACPI_DISABLE_OFFSET);

    /* PM1a/PM1b */
    g_pm1a_cnt = *(uint32_t *)((uintptr_t)fh + FADT_PM1A_CNT_BLK_OFFSET);
    g_pm1b_cnt = *(uint32_t *)((uintptr_t)fh + FADT_PM1B_CNT_BLK_OFFSET);

    /* PM1_CNT_LEN */
    g_acpi_valid = 0;
    if (parse_s5_from_dsdt(dsdt) == 0)
    {
        /* convert SLP_TYP values from 0..N into shifted values (<< 10) */
        g_slp_typa = (g_slp_typa & 0x7) << 10;
        g_slp_typb = (g_slp_typb & 0x7) << 10;
        g_acpi_valid = 1;
        return 0;
    }
    return -1;
}

/* to enable ACPI via SMI_CMD if needed (returns 0 if ACPI enabled or no-op) */
static int acpi_enable_if_needed(void)
{
    if (!g_acpi_valid)
        return -1;
    /* read PM1a to check SCI_EN (bit 0) */
    if (g_pm1a_cnt == 0)
        return -1;
    uint16_t pm1a = inw((uint16_t)g_pm1a_cnt);
    if ((pm1a & 1) == 0)
    {
        if (g_smi_cmd && g_acpi_enable)
        {
            outb((uint16_t)g_smi_cmd, g_acpi_enable);
            /* wait up to ~3s for SCI_EN to appear */
            for (int i = 0; i < 300; ++i)
            {
                uint16_t v = inw((uint16_t)g_pm1a_cnt);
                if (v & 1)
                    return 0;
                /* small delay (simple busy) */
                for (volatile int d = 0; d < 100000; ++d)
                    asm volatile("pause");
            }
            return -1;
        }
        else
        {
            /* no way to enable, but some BIOSes accept writing SLP_EN without enabling */
            return 0;
        }
    }
    return 0;
}

/* perform power off */
void acpi_poweroff(void)
{
    acpi_enable_if_needed();

    /* write PM1a */
    if (g_pm1a_cnt)
    {
        outw((uint16_t)g_pm1a_cnt, (uint16_t)(g_slp_typa | g_slp_en));
    }
    /* write PM1b if present */
    if (g_pm1b_cnt)
    {
        outw((uint16_t)g_pm1b_cnt, (uint16_t)(g_slp_typb | g_slp_en));
    }
}

void power_off(void)
{
    /* disable interrupts */
    asm volatile("cli");

    /* fallback - emulator ports (QEMU / Bochs / VirtualBox / Cloud Hypervisor) */
    outw(0x604, 0x2000);  /* QEMU */
    outw(0xB004, 0x2000); /* Bochs / old qemu */
    outw(0x4004, 0x3400); /* VirtualBox */
    outw(0x600, 0x34);    /* Cloud Hypervisor */

    if (acpi_init() == 0)
    {
        acpi_poweroff();
        /* small delay to allow poweroff to take effect */
        for (volatile int i = 0; i < 1000000; i++)
            asm volatile("pause");
    }

    /* final fallback: halt forever */
    for (;;)
        asm volatile("hlt");
}
