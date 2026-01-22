#include "pci.h"
#include "../../portio/portio.h"
#include "../../malloc/malloc.h"
#include "../../graphics/formatting/formatting.h"
#include <string.h>
#include <stdint.h>

#define CONFIG_ADDRESS_PORT 0xCF8
#define CONFIG_DATA_PORT 0xCFC

static pci_device_t *g_pci_devices = NULL;
static int g_pci_dev_count = 0;
static int g_pci_dev_capacity = 0;

static void ensure_capacity(void)
{
    if (!g_pci_devices)
    {
        g_pci_dev_capacity = 32;
        g_pci_devices = (pci_device_t *)malloc(sizeof(pci_device_t) * g_pci_dev_capacity);
        g_pci_dev_count = 0;
    }
    else if (g_pci_dev_count >= g_pci_dev_capacity)
    {
        int nc = g_pci_dev_capacity * 2;
        g_pci_devices = (pci_device_t *)realloc(g_pci_devices, sizeof(pci_device_t) * nc);
        g_pci_dev_capacity = nc;
    }
}

/* Формирование 32-битного адреса конфигурации (Mechanism #1) */
static uint32_t pci_conf_addr(uint8_t bus, uint8_t device, uint8_t function, uint8_t offset)
{
    return (uint32_t)((1u << 31) |
                      ((uint32_t)bus << 16) |
                      ((uint32_t)device << 11) |
                      ((uint32_t)function << 8) |
                      (offset & 0xFC));
}

/* Чтение / запись конфигурационного пространства */
static uint32_t pci_config_read32(uint8_t bus, uint8_t device, uint8_t function, uint8_t offset)
{
    uint32_t addr = pci_conf_addr(bus, device, function, offset);
    outl(CONFIG_ADDRESS_PORT, addr);
    return inl(CONFIG_DATA_PORT);
}

static void pci_config_write32(uint8_t bus, uint8_t device, uint8_t function, uint8_t offset, uint32_t value)
{
    uint32_t addr = pci_conf_addr(bus, device, function, offset);
    outl(CONFIG_ADDRESS_PORT, addr);
    outl(CONFIG_DATA_PORT, value);
}

static uint16_t pci_config_read16(uint8_t bus, uint8_t device, uint8_t function, uint8_t offset)
{
    uint32_t v = pci_config_read32(bus, device, function, offset & 0xFC);
    int shift = (offset & 2) * 8;
    return (uint16_t)((v >> shift) & 0xFFFF);
}

static uint8_t pci_config_read8(uint8_t bus, uint8_t device, uint8_t function, uint8_t offset)
{
    uint32_t v = pci_config_read32(bus, device, function, offset & 0xFC);
    int shift = (offset & 3) * 8;
    return (uint8_t)((v >> shift) & 0xFF);
}

/* Добавление устройства в список */
static void pci_register_device(const pci_device_t *dev)
{
    ensure_capacity();
    g_pci_devices[g_pci_dev_count++] = *dev;
}

/* Пробирование BAR: возвращает base и size (size==0 => не выделен) */
static void pci_probe_bars(pci_device_t *dev)
{
    /* Сохраним Command Register и временно выключим I/O и Memory responses */
    uint16_t cmd = pci_config_read16(dev->bus, dev->device, dev->function, 0x04);
    uint16_t cmd_saved = cmd;
    cmd &= ~(1u << 0); /* I/O Space */
    cmd &= ~(1u << 1); /* Memory Space */
    pci_config_write32(dev->bus, dev->device, dev->function, 0x04, (uint32_t)cmd);

    for (int i = 0; i < 6; ++i)
    {
        dev->bar_addr[i] = 0;
        dev->bar_size[i] = 0;
        dev->bar_is_io[i] = 0;

        uint8_t off = 0x10 + i * 4;
        uint32_t orig = pci_config_read32(dev->bus, dev->device, dev->function, off);
        if (orig == 0)
        {
            continue;
        }

        /* Определить тип */
        if (orig & 0x1)
        {
            /* I/O Space */
            dev->bar_is_io[i] = 1;
            pci_config_write32(dev->bus, dev->device, dev->function, off, 0xFFFFFFFF);
            uint32_t val = pci_config_read32(dev->bus, dev->device, dev->function, off);
            uint32_t mask = val & 0xFFFFFFFC;
            uint32_t size = (~mask) + 1;
            if (size == 0)
            {
                size = 0;
            }
            dev->bar_size[i] = (uint64_t)size;
            dev->bar_addr[i] = (uint64_t)(orig & 0xFFFFFFFC);
            pci_config_write32(dev->bus, dev->device, dev->function, off, orig);
        }
        else
        {
            /* Memory Space */
            uint32_t type = (orig >> 1) & 0x3;
            if (type == 0x2)
            {
                /* 64-bit BAR: consumes two entries */
                uint32_t orig_hi = pci_config_read32(dev->bus, dev->device, dev->function, off + 4);
                uint64_t full_orig = ((uint64_t)orig_hi << 32) | (orig & 0xFFFFFFF0);

                /* write all ones to both */
                pci_config_write32(dev->bus, dev->device, dev->function, off, 0xFFFFFFFF);
                pci_config_write32(dev->bus, dev->device, dev->function, off + 4, 0xFFFFFFFF);

                uint32_t val_lo = pci_config_read32(dev->bus, dev->device, dev->function, off) & 0xFFFFFFF0;
                uint32_t val_hi = pci_config_read32(dev->bus, dev->device, dev->function, off + 4);

                uint64_t mask = ((uint64_t)val_hi << 32) | val_lo;
                uint64_t size = (~mask) + 1;
                dev->bar_size[i] = size;
                dev->bar_addr[i] = full_orig;

                /* restore original low/high */
                pci_config_write32(dev->bus, dev->device, dev->function, off, (uint32_t)(full_orig & 0xFFFFFFFF));
                pci_config_write32(dev->bus, dev->device, dev->function, off + 4, (uint32_t)(full_orig >> 32));

                /* отметим следующий BAR как пропущенный (занят 64-bit) */
                ++i;
            }
            else
            {
                /* 32-bit memory BAR */
                pci_config_write32(dev->bus, dev->device, dev->function, off, 0xFFFFFFFF);
                uint32_t val = pci_config_read32(dev->bus, dev->device, dev->function, off);
                uint32_t mask = val & 0xFFFFFFF0;
                uint32_t size = (~mask) + 1;
                dev->bar_size[i] = (uint64_t)size;
                dev->bar_addr[i] = (uint64_t)(orig & 0xFFFFFFF0);
                /* restore */
                pci_config_write32(dev->bus, dev->device, dev->function, off, orig);
            }
        }
    }

    /* восстановим Command Register */
    pci_config_write32(dev->bus, dev->device, dev->function, 0x04, (uint32_t)cmd_saved);
}

/* Обрабатываем одну функцию устройства */
static void pci_check_function(uint8_t bus, uint8_t device, uint8_t function)
{
    uint16_t vendor = pci_config_read16(bus, device, function, 0x00);
    if (vendor == 0xFFFF)
        return;

    pci_device_t dev;
    memset(&dev, 0, sizeof(dev));
    dev.bus = bus;
    dev.device = device;
    dev.function = function;
    dev.vendor_id = vendor;
    dev.device_id = pci_config_read16(bus, device, function, 0x02);

    uint32_t reg = pci_config_read32(bus, device, function, 0x08);
    dev.class_code = (reg >> 24) & 0xFF;
    dev.subclass = (reg >> 16) & 0xFF;
    dev.prog_if = (reg >> 8) & 0xFF;

    dev.header_type = pci_config_read8(bus, device, function, 0x0C);

    /* Probe BARs */
    pci_probe_bars(&dev);

    /* Зарегистрируем устройство */
    pci_register_device(&dev);

    /* Если это PCI-to-PCI bridge (base class 6, subclass 4) - рекурсивно сканируем secondary bus */
    if (dev.class_code == 0x06 && dev.subclass == 0x04)
    {
        uint8_t secondary = pci_config_read8(bus, device, function, 0x19);
        if (secondary != 0)
        {
            /* рекурсивный проход */
            /* простая защита от глубокой рекурсии: если secondary == bus - пропускаем */
            if (secondary != bus)
            {
                extern void pci_scan_bus(uint8_t bus);
                pci_scan_bus(secondary);
            }
        }
    }
}

/* Обрабатываем устройство (включая мульти-Функции) */
static void pci_check_device(uint8_t bus, uint8_t device)
{
    uint16_t vendor = pci_config_read16(bus, device, 0, 0x00);
    if (vendor == 0xFFFF)
        return;

    /* Проверим header type для мультифункции */
    uint8_t header = pci_config_read8(bus, device, 0, 0x0C);
    pci_check_function(bus, device, 0);
    if (header & 0x80)
    {
        for (uint8_t f = 1; f < 8; ++f)
        {
            if (pci_config_read16(bus, device, f, 0x00) != 0xFFFF)
            {
                pci_check_function(bus, device, f);
            }
        }
    }
}

/* Сканирование одной шины: 32 устройства */
void pci_scan_bus(uint8_t bus)
{
    for (uint8_t dev = 0; dev < 32; ++dev)
    {
        pci_check_device(bus, dev);
    }
}

void pci_init(void)
{
    /* инициализация контейнера */
    if (!g_pci_devices)
    {
        ensure_capacity();
    }

    /* Проверка: если устройство 0:0:0 мультифункциональное, возможно несколько host controllers */
    uint8_t header = pci_config_read8(0, 0, 0, 0x0C);
    if ((header & 0x80) == 0)
    {
        /* single host controller */
        pci_scan_bus(0);
    }
    else
    {
        /* multi host controllers: функция i управляет шинами i */
        for (uint8_t f = 0; f < 8; ++f)
        {
            if (pci_config_read16(0, 0, f, 0x00) == 0xFFFF)
                break;
            pci_scan_bus(f);
        }
    }
}

int pci_get_device_count(void)
{
    return g_pci_dev_count;
}

pci_device_t *pci_get_device(int idx)
{
    if (idx < 0 || idx >= g_pci_dev_count)
        return NULL;
    return &g_pci_devices[idx];
}