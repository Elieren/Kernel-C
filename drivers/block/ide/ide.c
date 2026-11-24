#include "ide.h"
#include <asm/io.h>
#include <asm/pic.h>
#include <asm/cpu.h>
#include <asm/smp.h>
#include <asm/lapic.h>
#include <asm/ioapic.h>
#include <asm/idt.h>

/* Небольшая задержка: четыре чтения ALTSTATUS (~100ns) */
static inline void io_delay(uint16_t ctrl_port)
{
    (void)io_read8(ctrl_port + IDE_ALTSTATUS);
    (void)io_read8(ctrl_port + IDE_ALTSTATUS);
    (void)io_read8(ctrl_port + IDE_ALTSTATUS);
    (void)io_read8(ctrl_port + IDE_ALTSTATUS);
}

/* Ждём BSY=0 */
static int wait_bsy_clear(uint16_t base_port, uint16_t ctrl_port, uint32_t timeout)
{
    for (uint32_t i = 0; i < timeout; ++i)
    {
        uint8_t s = io_read8(base_port + IDE_STATUS);
        if (!(s & IDE_STATUS_BSY))
            return IDE_OK;
        if ((i & 0xFF) == 0)
            io_delay(ctrl_port);
    }
    return IDE_ERR_TIMEOUT;
}

/* Ждём DRQ=1 и BSY=0; если ERR — возвращаем ошибку */
static int wait_drq_or_err(uint16_t base_port, uint16_t ctrl_port, uint32_t timeout)
{
    for (uint32_t i = 0; i < timeout; ++i)
    {
        uint8_t s = io_read8(base_port + IDE_STATUS);
        if (s & IDE_STATUS_ERR)
            return IDE_ERR_DEVICE;
        if (!(s & IDE_STATUS_BSY) && (s & IDE_STATUS_DRQ))
            return IDE_OK;
        if ((i & 0xFF) == 0)
            io_delay(ctrl_port);
    }
    return IDE_ERR_TIMEOUT;
}

/* Проверить ERR и прочитать регистр ошибок */
static int check_err_and_clear(uint16_t base_port)
{
    uint8_t s = io_read8(base_port + IDE_STATUS);
    if (s & IDE_STATUS_ERR)
    {
        (void)io_read8(base_port + IDE_ERROR);
        return IDE_ERR_DEVICE;
    }
    return IDE_OK;
}

/* ===== Режим прерываний (IRQ14) =====
 * IRQ только будит ядро, готовность проверяется по ALTSTATUS.
 * IOAPIC: IRQ идёт на ядро, выдавшее команду; PIC: спит только BSP. */
#define IDE_ROUTE_NONE 0
#define IDE_ROUTE_PIC 1
#define IDE_ROUTE_IOAPIC 2

#define IDE_NOT_BOUND 0xFFFFFFFFu

/* Ожидаемый результат команды */
#define IDE_WAIT_DONE 0 /* без данных */
#define IDE_WAIT_DRQ 1  /* нужен DRQ */

static volatile int g_ide_irq_active[2];                   /* пока 0 — опрос */
static volatile int g_ide_route = IDE_ROUTE_NONE;          /* нужен для EOI */
static uint32_t g_ide_gsi;                                 /* GSI входа IRQ14 */
static volatile uint32_t g_ide_bound_apic = IDE_NOT_BOUND; /* APIC ID адресата */
static volatile uint32_t g_ide_irq_count[2];               /* диагностика */

static inline int ide_disk_irq_active(const ide_disk_t *disk)
{
    return g_ide_irq_active[disk->channel & 1];
}

/* Вызывается из isr_ide.asm; без локов, не трогает планировщик */
void ide_irq_handler(void)
{
    /* STATUS (не ALTSTATUS) снимает INTRQ */
    (void)io_read8(IDE_BASE_PRIMARY + IDE_STATUS);

    __atomic_fetch_add(&g_ide_irq_count[IDE_CHANNEL_PRIMARY], 1u, __ATOMIC_RELAXED);

    if (g_ide_route == IDE_ROUTE_IOAPIC)
        lapic_eoi(); /* EOI в LAPIC */
    else
        pic_send_eoi(IDE_IRQ_PRIMARY); /* EOI в PIC */
}

int ide_irq_enable(ide_channel_t channel)
{
    if (channel != IDE_CHANNEL_PRIMARY)
        return IDE_ERR_INVALID; /* вторичный канал остаётся на опросе */

    /* порядок: снять INTRQ, выбрать маршрут, включить IRQ в устройстве, открыть линию */
    (void)io_read8(IDE_BASE_PRIMARY + IDE_STATUS);

    int route = IDE_ROUTE_PIC;
    uint32_t gsi = 0;
    bool low = false, level = false;

    if (ioapic_present() && ioapic_resolve_isa_irq(IDE_IRQ_PRIMARY, &gsi, &low, &level))
    {
        /* стартовый адресат — BSP */
        uint32_t id = lapic_id();
        if (id <= IOAPIC_MAX_DEST_ID && ioapic_setup_route(gsi, IDE_PRIMARY, id, low, level))
        {
            g_ide_gsi = gsi;
            g_ide_bound_apic = id;
            route = IDE_ROUTE_IOAPIC;
        }
    }

    g_ide_route = route;
    io_write8(IDE_CTRL_PRIMARY + IDE_CONTROL, 0x00); /* nIEN = 0 */
    g_ide_irq_active[channel] = 1;

    if (route == IDE_ROUTE_IOAPIC)
    {
        pic_mask_irq(IDE_IRQ_PRIMARY); /* чтобы не пришло и через 8259 */
        ioapic_unmask(gsi);
    }
    else
    {
        pic_unmask_irq(IDE_IRQ_PRIMARY);
    }
    return IDE_OK;
}

void ide_irq_disable(ide_channel_t channel)
{
    if (channel != IDE_CHANNEL_PRIMARY)
        return;

    /* g_ide_route не трогаем: EOI позднего IRQ */
    g_ide_irq_active[channel] = 0;

    if (g_ide_route == IDE_ROUTE_IOAPIC)
        ioapic_mask(g_ide_gsi);
    pic_mask_irq(IDE_IRQ_PRIMARY);

    io_write8(IDE_CTRL_PRIMARY + IDE_CONTROL, IDE_CTRL_NIEN);
    (void)io_read8(IDE_BASE_PRIMARY + IDE_STATUS);
}

int ide_irq_active(ide_channel_t channel)
{
    return g_ide_irq_active[channel & 1];
}

uint32_t ide_irq_count(ide_channel_t channel)
{
    if (channel != IDE_CHANNEL_PRIMARY)
        return 0;
    return __atomic_load_n(&g_ide_irq_count[channel], __ATOMIC_RELAXED);
}

const char *ide_irq_route_name(ide_channel_t channel)
{
    if (channel != IDE_CHANNEL_PRIMARY || !g_ide_irq_active[channel])
        return "polling";
    return g_ide_route == IDE_ROUTE_IOAPIC ? "ioapic" : "pic";
}

/* Направить IRQ14 на текущее ядро, пока команда не выдана */
static void ide_bind_self(const ide_disk_t *disk)
{
    if (!ide_disk_irq_active(disk) || g_ide_route != IDE_ROUTE_IOAPIC)
        return;

    uint32_t id = lapic_id();
    if (id == g_ide_bound_apic)
        return;

    if (id <= IOAPIC_MAX_DEST_ID && ioapic_set_dest(g_ide_gsi, id))
        g_ide_bound_apic = id;
    else
        g_ide_bound_apic = IDE_NOT_BOUND; /* это ядро адресовать нельзя: будет опрос */
}

/* Дойдёт ли IRQ14 до этого ядра */
static int ide_can_sleep(void)
{
    switch (g_ide_route)
    {
    case IDE_ROUTE_IOAPIC:
        return lapic_id() == g_ide_bound_apic;
    case IDE_ROUTE_PIC:
        return smp_is_bsp();
    default:
        return 0;
    }
}

/* Спим до IRQ14 или тика; вытеснение запрещено: держим FS-лок и BKL */
static void ide_sleep_once(void)
{
    unsigned long flags = save_flags();

    preempt_disable();
    safe_halt();
    local_irq_disable();
    preempt_enable();
    restore_flags(flags);
}

/* Дождаться завершения команды (what: IDE_WAIT_*), не дольше max_wakes пробуждений */
static int ide_wait_complete(ide_disk_t *disk, int what, uint32_t max_wakes)
{
    if (!ide_disk_irq_active(disk) || !ide_can_sleep())
    {
        int rc = wait_bsy_clear(disk->base_port, disk->ctrl_port, IDE_TIMEOUT_LOOPS);
        if (rc != IDE_OK)
            return rc;
        if (what == IDE_WAIT_DRQ)
            return wait_drq_or_err(disk->base_port, disk->ctrl_port, IDE_TIMEOUT_LOOPS);
        return check_err_and_clear(disk->base_port);
    }

    /* 400 нс: иначе ALTSTATUS мог показать устаревшее «готово» */
    io_delay(disk->ctrl_port);

    for (uint32_t wakes = 0;; ++wakes)
    {
        if (!(io_read8(disk->ctrl_port + IDE_ALTSTATUS) & IDE_STATUS_BSY))
        {
            uint8_t s = io_read8(disk->base_port + IDE_STATUS); /* снимает INTRQ */
            if (s & (IDE_STATUS_ERR | IDE_STATUS_DF))
            {
                (void)io_read8(disk->base_port + IDE_ERROR);
                return IDE_ERR_DEVICE;
            }
            if (what == IDE_WAIT_DRQ && !(s & IDE_STATUS_DRQ))
                return IDE_ERR_DEVICE;
            return IDE_OK;
        }

        if (wakes >= max_wakes)
            return IDE_ERR_TIMEOUT;

        ide_sleep_once();
    }
}

/* Выбрать устройство (LBA/CHS) и задержка */
static void select_device_and_delay(uint16_t base, uint16_t ctrl, uint8_t drive, int lba_flag, uint8_t head_high4)
{
    uint8_t value = (lba_flag ? 0xE0 : 0xA0) | ((drive & 1) << 4) | (head_high4 & 0x0F);
    io_write8(base + IDE_SELECT, value);
    io_delay(ctrl);
}

/* Собрать 4 слова идентификатора в uint64_t (малый порядок слов) */
static uint64_t ident_words_to_u64(const uint16_t ident[256], int w)
{
    uint64_t v = 0;
    v |= (uint64_t)ident[w + 0];
    v |= (uint64_t)ident[w + 1] << 16;
    v |= (uint64_t)ident[w + 2] << 32;
    v |= (uint64_t)ident[w + 3] << 48;
    return v;
}

/* Прочитать один сектор (256 слов) в aligned_words */
static void read_sector_words_to(uint16_t base, uint16_t aligned_words[256])
{
    for (int i = 0; i < 256; ++i)
        aligned_words[i] = io_read16(base + IDE_DATA);
}

/* Записать один сектор (256 слов) из aligned_words */
static void write_sector_words_from(uint16_t base, const uint16_t aligned_words[256])
{
    for (int i = 0; i < 256; ++i)
        io_write16(base + IDE_DATA, aligned_words[i]);
}

/* IDENTIFY: заполняет ident_buffer и поля disk */
int ide_identify(ide_disk_t *disk, uint16_t ident_buffer[256])
{
    if (!disk || !ident_buffer)
        return IDE_ERR_INVALID;

    disk->type = IDE_TYPE_NONE;
    disk->supports_lba48 = 0;
    disk->sector_size = 512;
    disk->total_sectors = 0;

    /* Выбор устройства (CHS) */
    select_device_and_delay(disk->base_port, disk->ctrl_port, disk->drive, 0, 0);

    /* Отправка IDENTIFY */
    io_write8(disk->base_port + IDE_COMMAND, IDE_CMD_IDENTIFY);
    io_delay(disk->ctrl_port);

    uint8_t status = io_read8(disk->base_port + IDE_STATUS);
    if (status == 0)
        return IDE_ERR_DEVICE;

    /* Если ERR — возможно ATAPI */
    if (status & IDE_STATUS_ERR)
    {
        uint8_t cl = io_read8(disk->base_port + IDE_LCYL);
        uint8_t ch = io_read8(disk->base_port + IDE_HCYL);
        if ((cl == 0x14 && ch == 0xEB) || (cl == 0x69 && ch == 0x96))
        {
            io_write8(disk->base_port + IDE_COMMAND, IDE_CMD_IDENTIFY_PACKET);
            if (wait_bsy_clear(disk->base_port, disk->ctrl_port, IDE_TIMEOUT_LOOPS) != IDE_OK)
                return IDE_ERR_TIMEOUT;
            if (check_err_and_clear(disk->base_port) != IDE_OK)
                return IDE_ERR_DEVICE;
            if (wait_drq_or_err(disk->base_port, disk->ctrl_port, IDE_TIMEOUT_LOOPS) != IDE_OK)
                return IDE_ERR_TIMEOUT;
            for (int i = 0; i < 256; ++i)
                ident_buffer[i] = io_read16(disk->base_port + IDE_DATA);
            disk->type = IDE_TYPE_ATAPI;
            disk->sector_size = 2048;
            disk->total_sectors = 0;
            return IDE_OK;
        }
        else
        {
            return IDE_ERR_DEVICE;
        }
    }

    if (wait_bsy_clear(disk->base_port, disk->ctrl_port, IDE_TIMEOUT_LOOPS) != IDE_OK)
        return IDE_ERR_TIMEOUT;
    int rc = wait_drq_or_err(disk->base_port, disk->ctrl_port, IDE_TIMEOUT_LOOPS);
    if (rc != IDE_OK)
        return rc;

    for (int i = 0; i < 256; ++i)
        ident_buffer[i] = io_read16(disk->base_port + IDE_DATA);

    if (ident_buffer[0] == 0)
        return IDE_ERR_DEVICE;

    disk->type = IDE_TYPE_ATA;

    /* LBA28 (words 60-61) */
    uint32_t lba28 = ((uint32_t)ident_buffer[61] << 16) | ident_buffer[60];
    disk->total_sectors = lba28;

    /* Проверка LBA48 (word 83 bit 10) */
    if (ident_buffer[83] & (1u << 10))
    {
        disk->supports_lba48 = 1;
        uint64_t lba48 = ident_words_to_u64(ident_buffer, 100);
        if (lba48 != 0)
            disk->total_sectors = lba48;
    }
    else
    {
        disk->supports_lba48 = 0;
    }

    disk->sector_size = 512;
    return IDE_OK;
}

/* Инициализация структуры и вызов IDENTIFY */
int ide_init(ide_disk_t *disk, ide_channel_t channel, uint8_t drive)
{
    if (!disk || drive > 1)
        return IDE_ERR_INVALID;

    if (channel == IDE_CHANNEL_PRIMARY)
    {
        disk->base_port = IDE_BASE_PRIMARY;
        disk->ctrl_port = IDE_CTRL_PRIMARY;
    }
    else
    {
        disk->base_port = IDE_BASE_SECONDARY;
        disk->ctrl_port = IDE_CTRL_SECONDARY;
    }

    disk->drive = drive & 1;
    disk->channel = channel;
    disk->type = IDE_TYPE_NONE;
    disk->sector_size = 512;
    disk->supports_lba48 = 0;
    disk->total_sectors = 0;

    /* до включения IRQ устройство не должно поднимать INTRQ */
    if (!g_ide_irq_active[channel & 1])
        io_write8(disk->ctrl_port + IDE_CONTROL, IDE_CTRL_NIEN);

    uint16_t ident[256];
    int rc = ide_identify(disk, ident);
    if (rc != IDE_OK)
        return rc;

    return IDE_OK;
}

/* Настройка регистров для LBA28 */
static void setup_lba28_regs(uint16_t base, uint16_t ctrl, uint32_t lba, uint8_t count, uint8_t drive)
{
    select_device_and_delay(base, ctrl, drive, 1, (uint8_t)((lba >> 24) & 0x0F));
    io_write8(base + IDE_NSECT, count);
    io_write8(base + IDE_SECTOR, (uint8_t)(lba & 0xFF));
    io_write8(base + IDE_LCYL, (uint8_t)((lba >> 8) & 0xFF));
    io_write8(base + IDE_HCYL, (uint8_t)((lba >> 16) & 0xFF));
}

/* Настройка регистров для LBA48 */
static void setup_lba48_regs(uint16_t base, uint16_t ctrl, uint64_t lba, uint16_t count, uint8_t drive)
{
    select_device_and_delay(base, ctrl, drive, 1, (uint8_t)((lba >> 24) & 0x0F));
    io_delay(ctrl);

    io_write8(base + IDE_NSECT, (uint8_t)((count >> 8) & 0xFF)); /* SECCOUNT1 */
    io_write8(base + IDE_SECTOR, (uint8_t)((lba >> 24) & 0xFF)); /* LBA3 */
    io_write8(base + IDE_LCYL, (uint8_t)((lba >> 32) & 0xFF));   /* LBA4 */
    io_write8(base + IDE_HCYL, (uint8_t)((lba >> 40) & 0xFF));   /* LBA5 */

    io_write8(base + IDE_NSECT, (uint8_t)(count & 0xFF));      /* SECCOUNT0 */
    io_write8(base + IDE_SECTOR, (uint8_t)(lba & 0xFF));       /* LBA0 */
    io_write8(base + IDE_LCYL, (uint8_t)((lba >> 8) & 0xFF));  /* LBA1 */
    io_write8(base + IDE_HCYL, (uint8_t)((lba >> 16) & 0xFF)); /* LBA2 */
}

/* Проверка выравнивания по 2 байтам */
static inline int is_aligned_2(const void *ptr)
{
    return (((uintptr_t)ptr) & 1u) == 0;
}

/* Чтение секторов */
int ide_read_sectors(ide_disk_t *disk, uint64_t lba, uint32_t count, void *buffer)
{
    if (!disk || !buffer || count == 0)
        return IDE_ERR_INVALID;

    if (disk->total_sectors && lba > disk->total_sectors - (uint64_t)count)
        return IDE_ERR_INVALID;

    ide_bind_self(disk); /* IRQ14 на текущее ядро */

    const uint32_t max_per_op = 256;
    uint8_t *user_buf = (uint8_t *)buffer;
    uint32_t remaining = count;

    uint16_t tmp_sector_words[256];

    while (remaining)
    {
        uint32_t chunk = remaining > max_per_op ? max_per_op : remaining;

        for (uint32_t s = 0; s < chunk; ++s)
        {
            uint64_t cur_lba = lba + (count - remaining) + s;

            if (disk->supports_lba48 && (cur_lba > 0x0FFFFFFF))
            {
                setup_lba48_regs(disk->base_port, disk->ctrl_port, cur_lba, 1, disk->drive);
                io_write8(disk->base_port + IDE_COMMAND, IDE_CMD_READ_SECTORS_EXT);
            }
            else
            {
                uint32_t cur_lba32 = (uint32_t)cur_lba;
                setup_lba28_regs(disk->base_port, disk->ctrl_port, cur_lba32, 1, disk->drive);
                io_write8(disk->base_port + IDE_COMMAND, IDE_CMD_READ_SECTORS);
            }

            /* ждём IRQ14: сектор готов */
            int rc = ide_wait_complete(disk, IDE_WAIT_DRQ, IDE_IRQ_WAKES);
            if (rc != IDE_OK)
                return rc;

            int bytes_per_sector = disk->sector_size;
            int words_per_sector = bytes_per_sector / 2;

            uint8_t *dest = user_buf + ((count - remaining) + s) * (size_t)bytes_per_sector;
            if (is_aligned_2(dest) && bytes_per_sector == 512)
            {
                uint16_t *wptr = (uint16_t *)dest;
                for (int i = 0; i < words_per_sector; ++i)
                    wptr[i] = io_read16(disk->base_port + IDE_DATA);
            }
            else
            {
                if (bytes_per_sector == 512)
                {
                    read_sector_words_to(disk->base_port, tmp_sector_words);
                    memcpy(dest, tmp_sector_words, 512);
                }
                else
                {
                    for (int i = 0; i < words_per_sector; ++i)
                    {
                        uint16_t w = io_read16(disk->base_port + IDE_DATA);
                        dest[2 * i + 0] = (uint8_t)(w & 0xFF);
                        dest[2 * i + 1] = (uint8_t)((w >> 8) & 0xFF);
                    }
                }
            }
        }

        user_buf += (size_t)chunk * disk->sector_size;
        remaining -= chunk;
        lba += chunk;
    }

    return IDE_OK;
}

/* Запись секторов */
int ide_write_sectors(ide_disk_t *disk, uint64_t lba, uint32_t count, const void *buffer)
{
    if (!disk || !buffer || count == 0)
        return IDE_ERR_INVALID;

    if (disk->total_sectors && lba > disk->total_sectors - (uint64_t)count)
        return IDE_ERR_INVALID;

    ide_bind_self(disk); /* IRQ14 на текущее ядро */

    const uint32_t max_per_op = 256;
    const uint8_t *user_buf = (const uint8_t *)buffer;
    uint32_t remaining = count;
    int used_lba48 = 0;

    uint16_t tmp_sector_words[256];

    while (remaining)
    {
        uint32_t chunk = remaining > max_per_op ? max_per_op : remaining;

        for (uint32_t s = 0; s < chunk; ++s)
        {
            uint64_t cur_lba = lba + (count - remaining) + s;

            int use_lba48 = disk->supports_lba48 && (cur_lba > 0x0FFFFFFF);

            if (use_lba48)
            {
                setup_lba48_regs(disk->base_port, disk->ctrl_port, cur_lba, 1, disk->drive);
                io_write8(disk->base_port + IDE_COMMAND, IDE_CMD_WRITE_SECTORS_EXT);
                used_lba48 = 1;
            }
            else
            {
                uint32_t cur_lba32 = (uint32_t)cur_lba;
                setup_lba28_regs(disk->base_port, disk->ctrl_port, cur_lba32, 1, disk->drive);
                io_write8(disk->base_port + IDE_COMMAND, IDE_CMD_WRITE_SECTORS);
            }

            int rc = wait_bsy_clear(disk->base_port, disk->ctrl_port, IDE_TIMEOUT_LOOPS);
            if (rc != IDE_OK)
                return rc;

            rc = wait_drq_or_err(disk->base_port, disk->ctrl_port, IDE_TIMEOUT_LOOPS);
            if (rc != IDE_OK)
                return rc;

            int bytes_per_sector = disk->sector_size;
            int words_per_sector = bytes_per_sector / 2;
            const uint8_t *src = user_buf + ((count - remaining) + s) * (size_t)bytes_per_sector;

            if (is_aligned_2(src) && bytes_per_sector == 512)
            {
                const uint16_t *wptr = (const uint16_t *)src;
                for (int i = 0; i < words_per_sector; ++i)
                    io_write16(disk->base_port + IDE_DATA, wptr[i]);
            }
            else
            {
                if (bytes_per_sector == 512)
                {
                    memcpy(tmp_sector_words, src, 512);
                    write_sector_words_from(disk->base_port, tmp_sector_words);
                }
                else
                {
                    for (int i = 0; i < words_per_sector; ++i)
                    {
                        uint16_t w = (uint16_t)src[2 * i] | ((uint16_t)src[2 * i + 1] << 8);
                        io_write16(disk->base_port + IDE_DATA, w);
                    }
                }
            }

            /* ждём IRQ14: сектор записан */
            rc = ide_wait_complete(disk, IDE_WAIT_DONE, IDE_IRQ_WAKES);
            if (rc != IDE_OK)
                return rc;
        }

        user_buf += (size_t)chunk * disk->sector_size;
        remaining -= chunk;
        lba += chunk;
    }

    /* Flush cache */
    if (used_lba48)
        io_write8(disk->base_port + IDE_COMMAND, IDE_CMD_CACHE_FLUSH_EXT);
    else
        io_write8(disk->base_port + IDE_COMMAND, IDE_CMD_CACHE_FLUSH);

    /* самая долгая операция */
    return ide_wait_complete(disk, IDE_WAIT_DONE, IDE_IRQ_FLUSH_WAKES);
}
