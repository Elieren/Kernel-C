#include <boot/bootinfo.h>
#include <asm/io.h>
#include "kernel/power/power.h"
#include <string.h>
#include <stdint.h>
#include <asm/cpu.h>
#include "fs/fat16/fs.h"

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
    uint8_t reset_reg[12];
    uint8_t reset_value;
    uint16_t arm_boot_arch;
    uint8_t fadt_minor_version;
    uint64_t x_firmware_ctrl;
    uint64_t x_dsdt;
    uint8_t x_pm1a_event_block[12];
    uint8_t x_pm1b_event_block[12];
    uint8_t x_pm1a_control_block[12];
    uint8_t x_pm1b_control_block[12];
} __attribute__((packed)) fadt_t;

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
    {
        for (volatile int j = 0; j < 10000; j++)
        {
            cpu_relax();
        }
    }
}

/* ============================================================================
 * Утилиты для работы с ACPI
 * ============================================================================ */

static int acpi_checksum_valid(void *table, size_t length)
{
    uint8_t sum = 0;
    uint8_t *ptr = (uint8_t *)table;
    for (size_t i = 0; i < length; i++)
    {
        sum += ptr[i];
    }
    return sum == 0;
}

static void *find_acpi_table_rsdt(rsdt_t *rsdt, const char *signature)
{
    if (!rsdt)
        return NULL;
    acpi_sdt_header_t *header = &rsdt->header;
    uint32_t entries = (header->length - sizeof(acpi_sdt_header_t)) / 4;

    for (uint32_t i = 0; i < entries; i++)
    {
        uint32_t addr = rsdt->entries[i];
        if (addr == 0)
            continue;

        acpi_sdt_header_t *table = (acpi_sdt_header_t *)(uintptr_t)addr;
        if (memcmp(table->signature, signature, 4) == 0)
        {
            if (table->length >= sizeof(acpi_sdt_header_t) &&
                acpi_checksum_valid(table, table->length))
            {
                return table;
            }
        }
    }
    return NULL;
}

static void *find_acpi_table_xsdt(xsdt_t *xsdt, const char *signature)
{
    if (!xsdt)
        return NULL;
    acpi_sdt_header_t *header = &xsdt->header;
    uint32_t entries = (header->length - sizeof(acpi_sdt_header_t)) / 8;

    for (uint32_t i = 0; i < entries; i++)
    {
        uint64_t addr = xsdt->entries[i];
        if (addr == 0 || addr > 0xFFFFFFFFULL)
            continue;

        acpi_sdt_header_t *table = (acpi_sdt_header_t *)(uintptr_t)addr;
        if (memcmp(table->signature, signature, 4) == 0)
        {
            if (table->length >= sizeof(acpi_sdt_header_t) &&
                acpi_checksum_valid(table, table->length))
            {
                return table;
            }
        }
    }
    return NULL;
}

/* ============================================================================
 * Улучшенный парсер AML для поиска _S5 пакета
 * ============================================================================ */

static int parse_s5_package(uint8_t *pkg_start, uint32_t max_len,
                            uint8_t *slp_typa, uint8_t *slp_typb)
{
    uint32_t i = 0;

    if (i >= max_len)
        return 0;
    if (pkg_start[i] != 0x12)
        return 0;
    i++;

    if (i >= max_len)
        return 0;
    uint32_t pkg_length = pkg_start[i] & 0x3F;
    uint8_t byte_count = (pkg_start[i] >> 6) & 0x03;
    i++;

    for (uint8_t b = 0; b < byte_count && i < max_len; b++, i++)
    {
        pkg_length |= ((uint32_t)pkg_start[i]) << (8 * b + 4);
    }

    if (i >= max_len)
        return 0;
    uint8_t num_elements = pkg_start[i++];

    if (num_elements < 2)
        return 0;

    if (i >= max_len)
        return 0;
    if (pkg_start[i] == 0x0A)
    {
        i++;
        if (i >= max_len)
            return 0;
        *slp_typa = pkg_start[i++];
    }
    else if (pkg_start[i] == 0x00)
    {
        *slp_typa = 0;
        i++;
    }
    else if (pkg_start[i] == 0x01)
    {
        *slp_typa = 1;
        i++;
    }
    else if (pkg_start[i] == 0x0B)
    {
        i++;
        if (i + 1 >= max_len)
            return 0;
        *slp_typa = pkg_start[i];
        i += 2;
    }
    else
    {
        *slp_typa = pkg_start[i++] & 0x0F;
    }

    if (i >= max_len)
        return 0;
    if (pkg_start[i] == 0x0A)
    {
        i++;
        if (i >= max_len)
            return 0;
        *slp_typb = pkg_start[i++];
    }
    else if (pkg_start[i] == 0x00)
    {
        *slp_typb = 0;
        i++;
    }
    else if (pkg_start[i] == 0x01)
    {
        *slp_typb = 1;
        i++;
    }
    else if (pkg_start[i] == 0x0B)
    {
        i++;
        if (i + 1 >= max_len)
            return 0;
        *slp_typb = pkg_start[i];
        i += 2;
    }
    else
    {
        *slp_typb = pkg_start[i++] & 0x0F;
    }

    return 1;
}

static int find_s5_in_table(acpi_sdt_header_t *table, uint8_t *slp_typa, uint8_t *slp_typb)
{
    if (!table || table->length <= sizeof(acpi_sdt_header_t))
        return 0;

    uint8_t *aml = (uint8_t *)table + sizeof(acpi_sdt_header_t);
    uint32_t len = table->length - sizeof(acpi_sdt_header_t);

    for (uint32_t i = 0; i < len - 8; i++)
    {
        int found = 0;

        if (i + 4 <= len &&
            aml[i] == '_' && aml[i + 1] == 'S' &&
            aml[i + 2] == '5' && aml[i + 3] == '_')
        {
            found = 1;
            i += 4;
        }
        else if (i + 4 <= len &&
                 aml[i] == '\\' && aml[i + 1] == '_' &&
                 aml[i + 2] == 'S' && aml[i + 3] == '5')
        {
            found = 1;
            i += 4;
        }

        if (!found)
            continue;

        for (uint32_t j = 0; j < 64 && (i + j) < len; j++)
        {
            if (aml[i + j] == 0x12)
            {
                if (parse_s5_package(&aml[i + j], len - (i + j), slp_typa, slp_typb))
                {
                    return 1;
                }
                break;
            }
        }
    }
    return 0;
}

/* ============================================================================
 * ACPI выключение (основной метод)
 * ============================================================================ */

static int try_acpi_shutdown(void)
{
    boot_info_t *boot_info = get_boot_info();
    if (!boot_info || boot_info->rsdp_addr == 0)
        return 0;

    rsdp_v1_t *rsdp = (rsdp_v1_t *)(uintptr_t)boot_info->rsdp_addr;

    void *root_table = NULL;
    int use_xsdt = 0;

    if (rsdp->revision >= 2)
    {
        rsdp_v2_t *rsdp2 = (rsdp_v2_t *)rsdp;
        if (rsdp2->xsdt_address != 0 && rsdp2->xsdt_address <= 0xFFFFFFFFULL)
        {
            root_table = (void *)(uintptr_t)rsdp2->xsdt_address;
            use_xsdt = 1;
        }
    }

    if (root_table == NULL && rsdp->rsdt_address != 0)
    {
        root_table = (void *)(uintptr_t)rsdp->rsdt_address;
        use_xsdt = 0;
    }

    if (root_table == NULL)
        return 0;

    acpi_sdt_header_t *root_header = (acpi_sdt_header_t *)root_table;
    if (!acpi_checksum_valid(root_table, root_header->length))
        return 0;

    fadt_t *fadt = NULL;
    if (use_xsdt)
    {
        fadt = (fadt_t *)find_acpi_table_xsdt((xsdt_t *)root_table, "FACP");
    }
    else
    {
        fadt = (fadt_t *)find_acpi_table_rsdt((rsdt_t *)root_table, "FACP");
    }

    if (fadt == NULL)
        return 0;

    uint32_t pm1a_cnt = 0;
    uint32_t pm1b_cnt = 0;

    if (fadt->header.length >= 148)
    {
        uint8_t *fadt_data = (uint8_t *)fadt;
        uint8_t *x_pm1a_ptr = fadt_data + 148;

        uint8_t addr_space = x_pm1a_ptr[0];
        if (addr_space == 1)
        {
            uint64_t addr;
            memcpy(&addr, x_pm1a_ptr + 4, 8);
            if (addr <= 0xFFFF)
            {
                pm1a_cnt = (uint32_t)addr;
            }
        }

        uint8_t *x_pm1b_ptr = fadt_data + 160;
        addr_space = x_pm1b_ptr[0];
        if (addr_space == 1)
        {
            uint64_t addr;
            memcpy(&addr, x_pm1b_ptr + 4, 8);
            if (addr <= 0xFFFF)
            {
                pm1b_cnt = (uint32_t)addr;
            }
        }
    }

    if (pm1a_cnt == 0)
    {
        pm1a_cnt = fadt->pm1a_control_block;
    }
    if (pm1b_cnt == 0)
    {
        pm1b_cnt = fadt->pm1b_control_block;
    }

    if (pm1a_cnt == 0)
        return 0;

    acpi_sdt_header_t *dsdt = NULL;

    if (fadt->header.length >= 140)
    {
        uint8_t *fadt_data = (uint8_t *)fadt;
        uint64_t x_dsdt_addr;
        memcpy(&x_dsdt_addr, fadt_data + 140, 8);

        if (x_dsdt_addr != 0 && x_dsdt_addr <= 0xFFFFFFFFULL)
        {
            dsdt = (acpi_sdt_header_t *)(uintptr_t)x_dsdt_addr;
        }
    }

    if (dsdt == NULL && fadt->dsdt != 0)
    {
        dsdt = (acpi_sdt_header_t *)(uintptr_t)fadt->dsdt;
    }

    uint8_t slp_typa = 5;
    uint8_t slp_typb = 5;

    if (dsdt && acpi_checksum_valid(dsdt, dsdt->length))
    {
        find_s5_in_table(dsdt, &slp_typa, &slp_typb);
    }

    if (!find_s5_in_table(dsdt, &slp_typa, &slp_typb))
    {
        if (use_xsdt)
        {
            acpi_sdt_header_t *ssdt = (acpi_sdt_header_t *)find_acpi_table_xsdt((xsdt_t *)root_table, "SSDT");
            if (ssdt)
            {
                find_s5_in_table(ssdt, &slp_typa, &slp_typb);
            }
        }
        else
        {
            acpi_sdt_header_t *ssdt = (acpi_sdt_header_t *)find_acpi_table_rsdt((rsdt_t *)root_table, "SSDT");
            if (ssdt)
            {
                find_s5_in_table(ssdt, &slp_typa, &slp_typb);
            }
        }
    }

    if (fadt->smi_command_port != 0 && fadt->acpi_enable != 0)
    {
        uint16_t pm1_sts = io_read16((uint16_t)pm1a_cnt);
        if ((pm1_sts & 1) == 0)
        {
            io_write8((uint16_t)fadt->smi_command_port, fadt->acpi_enable);
            delay_ms(100);
        }
    }

    uint16_t pm1a_value = (1 << 13) | ((uint16_t)(slp_typa & 0x7) << 10);
    uint16_t pm1b_value = (1 << 13) | ((uint16_t)(slp_typb & 0x7) << 10);

    local_irq_disable();

    io_write16((uint16_t)pm1a_cnt, pm1a_value);
    io_wait();

    if (pm1b_cnt != 0)
    {
        io_write16((uint16_t)pm1b_cnt, pm1b_value);
        io_wait();
    }

    delay_ms(1000);

    return 0;
}

/* ============================================================================
 * Метод 2: Brute-force ACPI
 * ============================================================================ */

static int try_acpi_bruteforce(void)
{
    boot_info_t *boot_info = get_boot_info();
    if (!boot_info || boot_info->rsdp_addr == 0)
        return 0;

    rsdp_v1_t *rsdp = (rsdp_v1_t *)(uintptr_t)boot_info->rsdp_addr;
    if (rsdp->rsdt_address == 0)
        return 0;

    rsdt_t *rsdt = (rsdt_t *)(uintptr_t)rsdp->rsdt_address;
    fadt_t *fadt = (fadt_t *)find_acpi_table_rsdt(rsdt, "FACP");

    if (fadt == NULL || fadt->pm1a_control_block == 0)
        return 0;

    uint16_t pm1a_cnt = (uint16_t)fadt->pm1a_control_block;
    uint16_t pm1b_cnt = (uint16_t)fadt->pm1b_control_block;

    local_irq_disable();

    uint8_t slp_values[] = {5, 7, 0, 6, 4, 3, 2, 1};

    for (int i = 0; i < 8; i++)
    {
        uint16_t value = (1 << 13) | ((uint16_t)slp_values[i] << 10);
        io_write16(pm1a_cnt, value);
        if (pm1b_cnt != 0)
        {
            io_write16(pm1b_cnt, value);
        }
        delay_ms(100);
    }

    return 0;
}

/* ============================================================================
 * Метод 3: QEMU/Bochs/VirtualBox/Cloud Hypervisor специальные порты
 * ============================================================================ */

static void try_qemu_shutdown(void)
{
    io_write16(0x604, 0x2000);
    delay_ms(100);

    io_write16(0xB004, 0x2000);
    delay_ms(100);

    io_write8(0xf4, 0x00);
    delay_ms(100);
}

static void try_virtualbox_shutdown(void)
{
    io_write16(0x4004, 0x3400);
    delay_ms(100);
}

static void try_cloud_hypervisor_shutdown(void)
{
    io_write16(0x600, 0x34);
    delay_ms(100);
}

/* ============================================================================
 * Метод 4: Port 0xCF9 (PCI Reset)
 * ============================================================================ */

static void try_pci_reset(void)
{
    uint8_t temp = io_read8(0xCF9);
    temp |= 0x04;
    temp |= 0x02;
    io_write8(0xCF9, temp);
    delay_ms(100);

    io_write8(0xCF9, 0x0E);
    delay_ms(100);

    io_write8(0xCF9, 0x06);
    delay_ms(100);
}

/* ============================================================================
 * Метод 5: Keyboard Controller
 * ============================================================================ */

static void try_keyboard_shutdown(void)
{
    uint8_t temp;

    for (int i = 0; i < 1000; i++)
    {
        temp = io_read8(0x64);
        if ((temp & 0x02) == 0)
            break;
        io_wait();
    }

    io_write8(0x64, 0xFE);
    delay_ms(500);

    for (int i = 0; i < 1000; i++)
    {
        temp = io_read8(0x64);
        if ((temp & 0x02) == 0)
            break;
        io_wait();
    }
    io_write8(0x64, 0xD1);

    for (int i = 0; i < 1000; i++)
    {
        temp = io_read8(0x64);
        if ((temp & 0x02) == 0)
            break;
        io_wait();
    }
    io_write8(0x60, 0x00);
    delay_ms(500);
}

/* ============================================================================
 * Метод 6: PS/2 Controller
 * ============================================================================ */

static void try_ps2_shutdown(void)
{
    while (io_read8(0x64) & 0x02)
        io_wait();
    io_write8(0x64, 0xAD);

    while (io_read8(0x64) & 0x02)
        io_wait();
    io_write8(0x64, 0xA7);

    while (io_read8(0x64) & 0x02)
        io_wait();
    io_write8(0x64, 0xD0);

    while ((io_read8(0x64) & 0x01) == 0)
        io_wait();
    uint8_t port = io_read8(0x60);

    while (io_read8(0x64) & 0x02)
        io_wait();
    io_write8(0x64, 0xD1);

    while (io_read8(0x64) & 0x02)
        io_wait();
    io_write8(0x60, port & ~0x01);

    delay_ms(500);
}

/* ============================================================================
 * Метод 7: Triple Fault
 * ============================================================================ */

static void __attribute__((noreturn)) try_triple_fault(void)
{
    struct
    {
        uint16_t limit;
        uint64_t base;
    } __attribute__((packed)) idtr = {0, 0};

    __asm__ volatile(
        "cli\n"
        "lidt %0\n"
        "int $0x00\n"
        : : "m"(idtr));

    while (1)
    {
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

    try_qemu_shutdown();
    delay_ms(300);

    try_virtualbox_shutdown();
    delay_ms(300);

    try_cloud_hypervisor_shutdown();
    delay_ms(300);

    try_pci_reset();
    delay_ms(500);

    try_keyboard_shutdown();
    delay_ms(500);

    try_ps2_shutdown();
    delay_ms(500);

    try_triple_fault();

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