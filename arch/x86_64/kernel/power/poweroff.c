#include <boot/bootinfo.h>
#include <asm/io.h>
#include "kernel/power/power.h"
#include <string.h>
#include <stdint.h>
#include <asm/cpu.h>
#include "fs/fat16/fs.h"

typedef struct
{
    uint8_t address_space;
    uint8_t bit_width;
    uint8_t bit_offset;
    uint8_t access_size;
    uint64_t address;
} __attribute__((packed)) gas_t;

/* ============================================================================
 * ACPI структуры
 * ============================================================================ */

typedef struct
{
    char signature[8];
    uint8_t checksum;
    char oem_id[6];
    uint8_t revision;
    uint32_t rsdt_address;
} __attribute__((packed)) rsdp_v1_t;

typedef struct
{
    rsdp_v1_t v1;
    uint32_t length;
    uint64_t xsdt_address;
    uint8_t extended_checksum;
    uint8_t reserved[3];
} __attribute__((packed)) rsdp_v2_t;

typedef struct
{
    char signature[4];
    uint32_t length;
    uint8_t revision;
    uint8_t checksum;
    char oem_id[6];
    char oem_table_id[8];
    uint32_t oem_revision;
    uint32_t creator_id;
    uint32_t creator_revision;
} __attribute__((packed)) acpi_sdt_header_t;

typedef struct
{
    acpi_sdt_header_t header;
    uint32_t entries[];
} __attribute__((packed)) rsdt_t;

typedef struct
{
    acpi_sdt_header_t header;
    uint64_t entries[];
} __attribute__((packed)) xsdt_t;

typedef struct
{
    acpi_sdt_header_t header;
    uint32_t firmware_ctrl;
    uint32_t dsdt;
    uint8_t reserved;
    uint8_t preferred_pm_profile;
    uint16_t sci_interrupt;
    uint32_t smi_command_port;
    uint8_t acpi_enable;
    uint8_t acpi_disable;
    uint8_t s4bios_req;
    uint8_t pstate_control;
    uint32_t pm1a_event_block;
    uint32_t pm1b_event_block;
    uint32_t pm1a_control_block;
    uint32_t pm1b_control_block;
    uint32_t pm2_control_block;
    uint32_t pm_timer_block;
    uint32_t gpe0_block;
    uint32_t gpe1_block;
    uint8_t pm1_event_length;
    uint8_t pm1_control_length;
    uint8_t pm2_control_length;
    uint8_t pm_timer_length;
    uint8_t gpe0_length;
    uint8_t gpe1_length;
    uint8_t gpe1_base;
    uint8_t cstate_control;
    uint16_t worst_c2_latency;
    uint16_t worst_c3_latency;
    uint16_t flush_size;
    uint16_t flush_stride;
    uint8_t duty_offset;
    uint8_t duty_width;
    uint8_t day_alarm;
    uint8_t month_alarm;
    uint8_t century;
    uint16_t boot_arch_flags;
    uint8_t reserved2;
    uint32_t flags;
    gas_t reset_reg;
    uint8_t reset_value;
    uint16_t arm_boot_arch;
    uint8_t fadt_minor_version;
    uint64_t x_firmware_ctrl;
    uint64_t x_dsdt;
    gas_t x_pm1a_event_block;
    gas_t x_pm1b_event_block;
    gas_t x_pm1a_control_block;
    gas_t x_pm1b_control_block;
    gas_t x_pm2_control_block;
    gas_t x_pm_timer_block;
    gas_t x_gpe0_block;
    gas_t x_gpe1_block;
    gas_t sleep_control_reg;
    gas_t sleep_status_reg;
    uint64_t hypervisor_vendor_id;
} __attribute__((packed)) fadt_t;

/* ============================================================================
 * Константы PM1_CNT
 * ============================================================================ */
#define PM1_CNT_SCI_EN (1u << 0)
#define PM1_CNT_SLP_TYP_IDX 10
#define PM1_CNT_SLP_TYP_MASK (7u << PM1_CNT_SLP_TYP_IDX)
#define PM1_CNT_SLP_EN (1u << 13)

#define PM1_CNT_PRESERVE_MASK ((1u << 3) | (1u << 4) | (1u << 5) | (1u << 6) | \
                               (1u << 7) | (1u << 8) | (1u << 9) |             \
                               (1u << 14) | (1u << 15))

#define PM1_STS_WAK_STS (1u << 15)

#define FADT_HW_REDUCED_ACPI (1u << 20)

/* ============================================================================
 * Константы SLEEP_CONTROL_REG для HW-reduced ACPI
 * ============================================================================ */
#define SLP_CNT_SLP_TYP_IDX 2
#define SLP_CNT_SLP_TYP_MASK (7u << SLP_CNT_SLP_TYP_IDX)
#define SLP_CNT_SLP_EN (1u << 5)

#define ACPI_SLP_TYP_MAX 0x7

/* ============================================================================
 * Задержки
 * ============================================================================ */

static inline void io_wait(void)
{
    io_write8(0x80, 0);
}

static void delay_ms(int ms)
{
    for (int i = 0; i < ms; i++)
        for (volatile int j = 0; j < 10000; j++)
            cpu_relax();
}

/* ============================================================================
 * Утилиты для работы с ACPI
 * ============================================================================ */

static int acpi_checksum_valid(void *table, size_t length)
{
    uint8_t sum = 0;
    uint8_t *ptr = (uint8_t *)table;
    for (size_t i = 0; i < length; i++)
        sum += ptr[i];
    return sum == 0;
}

static void *find_table_rsdt(rsdt_t *rsdt, const char *sig)
{
    if (!rsdt)
        return NULL;
    uint32_t n = (rsdt->header.length - sizeof(acpi_sdt_header_t)) / 4;
    for (uint32_t i = 0; i < n; i++)
    {
        uint32_t addr = rsdt->entries[i];
        if (!addr)
            continue;
        acpi_sdt_header_t *t = (acpi_sdt_header_t *)(uintptr_t)addr;
        if (memcmp(t->signature, sig, 4) == 0 &&
            t->length >= sizeof(acpi_sdt_header_t) &&
            acpi_checksum_valid(t, t->length))
            return t;
    }
    return NULL;
}

static void *find_table_xsdt(xsdt_t *xsdt, const char *sig)
{
    if (!xsdt)
        return NULL;
    uint32_t n = (xsdt->header.length - sizeof(acpi_sdt_header_t)) / 8;
    for (uint32_t i = 0; i < n; i++)
    {
        uint64_t addr = xsdt->entries[i];
        if (!addr || addr > 0xFFFFFFFFULL)
            continue;
        acpi_sdt_header_t *t = (acpi_sdt_header_t *)(uintptr_t)addr;
        if (memcmp(t->signature, sig, 4) == 0 &&
            t->length >= sizeof(acpi_sdt_header_t) &&
            acpi_checksum_valid(t, t->length))
            return t;
    }
    return NULL;
}

static uint32_t gas_get_io_port(const gas_t *gas)
{
    if (!gas || gas->address_space != 1)
        return 0;
    if (!gas->address || gas->address > 0xFFFF)
        return 0;
    return (uint32_t)gas->address;
}

/* ============================================================================
 * Парсер AML для поиска объекта _S5_
 * ============================================================================ */

static int parse_s5_package(uint8_t *p, uint32_t max_len,
                            uint8_t *slp_typa, uint8_t *slp_typb)
{
    uint32_t i = 0;

    if (i >= max_len || p[i] != 0x12)
        return 0;
    i++;

    if (i >= max_len)
        return 0;
    uint8_t extra = (p[i] >> 6) & 0x03;
    i++;
    for (uint8_t b = 0; b < extra && i < max_len; b++, i++)
        ;

    if (i >= max_len || p[i++] < 2)
        return 0;

    if (i >= max_len)
        return 0;
    if (p[i] == 0x0A)
    {
        i++;
        if (i >= max_len)
            return 0;
        *slp_typa = p[i++];
    }
    else if (p[i] == 0x0B)
    {
        i++;
        if (i + 1 >= max_len)
            return 0;
        *slp_typa = p[i];
        i += 2;
    }
    else if (p[i] <= 0x0F)
    {
        *slp_typa = p[i++];
    }
    else
        return 0;

    if (i >= max_len)
        return 0;
    if (p[i] == 0x0A)
    {
        i++;
        if (i >= max_len)
            return 0;
        *slp_typb = p[i++];
    }
    else if (p[i] == 0x0B)
    {
        i++;
        if (i + 1 >= max_len)
            return 0;
        *slp_typb = p[i];
        i += 2;
    }
    else if (p[i] <= 0x0F)
    {
        *slp_typb = p[i++];
    }
    else
        return 0;

    if (*slp_typa > ACPI_SLP_TYP_MAX || *slp_typb > ACPI_SLP_TYP_MAX)
        return 0;

    return 1;
}

static int find_s5_in_table(acpi_sdt_header_t *table,
                            uint8_t *slp_typa, uint8_t *slp_typb)
{
    if (!table || table->length <= sizeof(acpi_sdt_header_t))
        return 0;
    uint8_t *aml = (uint8_t *)table + sizeof(acpi_sdt_header_t);
    uint32_t len = table->length - (uint32_t)sizeof(acpi_sdt_header_t);

    for (uint32_t i = 0; i + 4 <= len; i++)
    {
        int adv = 0;
        if (aml[i] == '_' && aml[i + 1] == 'S' && aml[i + 2] == '5' && aml[i + 3] == '_')
            adv = 4;
        else if (i + 5 <= len && aml[i] == '\\' && aml[i + 1] == '_' &&
                 aml[i + 2] == 'S' && aml[i + 3] == '5' && aml[i + 4] == '_')
            adv = 5;

        if (!adv)
            continue;
        i += (uint32_t)adv;

        for (uint32_t j = 0; j < 64 && (i + j) < len; j++)
        {
            if (aml[i + j] == 0x12)
            {
                if (parse_s5_package(&aml[i + j], len - (i + j), slp_typa, slp_typb))
                    return 1;
                break;
            }
        }
    }
    return 0;
}

/* ============================================================================
 * Включение режима ACPI (SMI → SCI)
 * ============================================================================ */

static void acpi_enable_mode(fadt_t *fadt, uint32_t pm1a_cnt)
{
    if (io_read16((uint16_t)pm1a_cnt) & PM1_CNT_SCI_EN)
        return;
    if (!fadt->smi_command_port || !fadt->acpi_enable)
        return;

    io_write8((uint16_t)fadt->smi_command_port, fadt->acpi_enable);

    for (int ms = 0; ms < 3000; ms++)
    {
        if (io_read16((uint16_t)pm1a_cnt) & PM1_CNT_SCI_EN)
            return;
        delay_ms(1);
    }
}

/* ============================================================================
 * Очистка WAK_STS
 * ============================================================================ */

static void acpi_clear_wak_sts(fadt_t *fadt)
{
    if (fadt->pm1a_event_block)
        io_write16((uint16_t)fadt->pm1a_event_block, PM1_STS_WAK_STS);
    if (fadt->pm1b_event_block)
        io_write16((uint16_t)fadt->pm1b_event_block, PM1_STS_WAK_STS);
}

/* ============================================================================
 * Запись в PM1_CNT
 * ============================================================================ */

static void pm1_cnt_do_sleep(uint32_t port_a, uint32_t port_b,
                             uint8_t slp_typa, uint8_t slp_typb)
{
    if (!port_a)
        return;

    uint16_t base = io_read16((uint16_t)port_a);

#define PM1_CNT_GBL_RLS (1u << 2)
    base &= (uint16_t)~(PM1_CNT_SLP_TYP_MASK | PM1_CNT_SLP_EN | PM1_CNT_GBL_RLS);

    uint16_t pm1a = base | (uint16_t)((slp_typa & 0x7) << PM1_CNT_SLP_TYP_IDX);
    uint16_t pm1b = base | (uint16_t)((slp_typb & 0x7) << PM1_CNT_SLP_TYP_IDX);

    io_write16((uint16_t)port_a, pm1a);
    io_wait();
    if (port_b)
    {
        io_write16((uint16_t)port_b, pm1b);
        io_wait();
    }

    pm1a |= PM1_CNT_SLP_EN;
    pm1b |= PM1_CNT_SLP_EN;

    io_write16((uint16_t)port_a, pm1a);
    io_wait();
    if (port_b)
    {
        io_write16((uint16_t)port_b, pm1b);
        io_wait();
    }
}

/* ============================================================================
 * HW-reduced ACPI shutdown
 * ============================================================================ */

static void try_hw_reduced_shutdown(fadt_t *fadt, uint8_t slp_typ)
{
    if (!(fadt->flags & FADT_HW_REDUCED_ACPI))
        return;
    if (fadt->header.length < 256)
        return;

    uint32_t ctrl_port = gas_get_io_port(&fadt->sleep_control_reg);
    if (!ctrl_port)
        return;

    uint8_t ctrl = (uint8_t)(((slp_typ << SLP_CNT_SLP_TYP_IDX) & SLP_CNT_SLP_TYP_MASK) |
                             SLP_CNT_SLP_EN);

    io_write8((uint16_t)ctrl_port, ctrl);
    io_wait();
    delay_ms(1000);
}

/* ============================================================================
 * Метод 1: Полный ACPI shutdown (RSDP → XSDT/RSDT → FADT → DSDT/_S5)
 * ============================================================================ */

static int try_acpi_shutdown(void)
{
    boot_info_t *boot_info = get_boot_info();
    if (!boot_info || !boot_info->rsdp_addr)
        return 0;

    rsdp_v1_t *rsdp = (rsdp_v1_t *)(uintptr_t)boot_info->rsdp_addr;

    void *root_table = NULL;
    int use_xsdt = 0;

    if (rsdp->revision >= 2)
    {
        rsdp_v2_t *rsdp2 = (rsdp_v2_t *)rsdp;
        if (rsdp2->xsdt_address && rsdp2->xsdt_address <= 0xFFFFFFFFULL)
        {
            root_table = (void *)(uintptr_t)rsdp2->xsdt_address;
            use_xsdt = 1;
        }
    }
    if (!root_table && rsdp->rsdt_address)
        root_table = (void *)(uintptr_t)rsdp->rsdt_address;
    if (!root_table)
        return 0;

    acpi_sdt_header_t *root_hdr = (acpi_sdt_header_t *)root_table;
    if (!acpi_checksum_valid(root_table, root_hdr->length))
        return 0;

    fadt_t *fadt = use_xsdt
                       ? (fadt_t *)find_table_xsdt((xsdt_t *)root_table, "FACP")
                       : (fadt_t *)find_table_rsdt((rsdt_t *)root_table, "FACP");
    if (!fadt)
        return 0;

    uint32_t pm1a_cnt = 0;
    uint32_t pm1b_cnt = 0;

    if (fadt->header.length >= 196)
    {
        uint32_t p = gas_get_io_port(&fadt->x_pm1a_control_block);
        if (p)
            pm1a_cnt = p;
        p = gas_get_io_port(&fadt->x_pm1b_control_block);
        if (p)
            pm1b_cnt = p;
    }
    if (!pm1a_cnt)
        pm1a_cnt = fadt->pm1a_control_block;
    if (!pm1b_cnt)
        pm1b_cnt = fadt->pm1b_control_block;
    if (!pm1a_cnt)
        return 0;

    acpi_sdt_header_t *dsdt = NULL;

    if (fadt->header.length >= 148 && fadt->x_dsdt &&
        fadt->x_dsdt <= 0xFFFFFFFFULL)
        dsdt = (acpi_sdt_header_t *)(uintptr_t)fadt->x_dsdt;
    if (!dsdt && fadt->dsdt)
        dsdt = (acpi_sdt_header_t *)(uintptr_t)fadt->dsdt;

    uint8_t slp_typa = 5;
    uint8_t slp_typb = 5;

    if (!dsdt || !acpi_checksum_valid(dsdt, dsdt->length) ||
        !find_s5_in_table(dsdt, &slp_typa, &slp_typb))
    {
        acpi_sdt_header_t *ssdt = use_xsdt
                                      ? (acpi_sdt_header_t *)find_table_xsdt((xsdt_t *)root_table, "SSDT")
                                      : (acpi_sdt_header_t *)find_table_rsdt((rsdt_t *)root_table, "SSDT");
        if (ssdt)
            find_s5_in_table(ssdt, &slp_typa, &slp_typb);
    }

    acpi_enable_mode(fadt, pm1a_cnt);

    acpi_clear_wak_sts(fadt);

    local_irq_disable();

    try_hw_reduced_shutdown(fadt, slp_typa);

    pm1_cnt_do_sleep(pm1a_cnt, pm1b_cnt, slp_typa, slp_typb);

    delay_ms(10000);
    return 0;
}

/* ============================================================================
 * Метод 2: ACPI brute-force (без парсинга DSDT, перебираем SLP_TYP)
 * ============================================================================ */

static int try_acpi_bruteforce(void)
{
    boot_info_t *boot_info = get_boot_info();
    if (!boot_info || !boot_info->rsdp_addr)
        return 0;

    rsdp_v1_t *rsdp = (rsdp_v1_t *)(uintptr_t)boot_info->rsdp_addr;
    if (!rsdp->rsdt_address)
        return 0;

    rsdt_t *rsdt = (rsdt_t *)(uintptr_t)rsdp->rsdt_address;
    fadt_t *fadt = (fadt_t *)find_table_rsdt(rsdt, "FACP");
    if (!fadt || !fadt->pm1a_control_block)
        return 0;

    uint32_t pm1a = fadt->pm1a_control_block;
    uint32_t pm1b = fadt->pm1b_control_block;

    acpi_enable_mode(fadt, pm1a);
    acpi_clear_wak_sts(fadt);
    local_irq_disable();

    uint8_t slp_values[] = {5, 7, 6, 4, 3, 2, 1, 0};
    for (int i = 0; i < 8; i++)
    {
        pm1_cnt_do_sleep(pm1a, pm1b, slp_values[i], slp_values[i]);
        delay_ms(200);
    }
    return 0;
}

/* ============================================================================
 * Метод 3: Эмулятор-специфичные порты (QEMU/Bochs/VirtualBox/Cloud Hypervisor)
 * ============================================================================ */

static void try_emulator_shutdown(void)
{
    io_write16(0x604, 0x2000);
    io_wait();
    delay_ms(100);

    io_write16(0xB004, 0x2000);
    io_wait();
    delay_ms(100);

    io_write16(0x4004, 0x3400);
    io_wait();
    delay_ms(100);

    io_write16(0x600, 0x34);
    io_wait();
    delay_ms(100);
}

/* ============================================================================
 * Метод 4: Port 0xCF9 - Full PCI/CPU Reset
 * ============================================================================ */

static void try_cf9_reset(void)
{
    uint8_t v = io_read8(0xCF9) & ~0x0E;
    io_write8(0xCF9, v | 0x0A);
    io_wait();
    io_write8(0xCF9, v | 0x0E);
    io_wait();
    delay_ms(500);
    io_write8(0xCF9, 0x06);
    io_wait();
    delay_ms(500);
}

/* ============================================================================
 * Метод 5: Keyboard Controller Reset (PS/2)
 * ============================================================================ */

static void try_keyboard_reset(void)
{
    for (int i = 0; i < 10000; i++)
    {
        if (!(io_read8(0x64) & 0x02))
            break;
        io_wait();
    }
    io_write8(0x64, 0xFE);
    delay_ms(500);
}

/* ============================================================================
 * Метод 6: Triple Fault - гарантированная остановка
 * ============================================================================ */

static void __attribute__((noreturn)) do_triple_fault(void)
{
    struct
    {
        uint16_t limit;
        uint64_t base;
    } __attribute__((packed)) idtr_null = {0, 0};

    __asm__ volatile(
        "cli\n\t"
        "lidt %0\n\t"
        "int $0x00\n\t"
        :
        : "m"(idtr_null)
        : "memory");

    while (1)
    {
        local_irq_disable();
        halt();
    }
}

/* ============================================================================
 * ГЛАВНАЯ ФУНКЦИЯ
 * ============================================================================ */

void __attribute__((noreturn)) universal_shutdown(void)
{
    try_acpi_shutdown();
    delay_ms(500);

    try_acpi_bruteforce();
    delay_ms(500);

    try_emulator_shutdown();
    delay_ms(500);

    try_cf9_reset();
    delay_ms(500);

    try_keyboard_reset();
    delay_ms(500);

    do_triple_fault();

    while (1)
    {
        local_irq_disable();
        halt();
    }
}

void power_off(void)
{
    fs_sync();
    universal_shutdown();
}