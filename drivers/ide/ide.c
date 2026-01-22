#include "ide.h"
#include "../../portio/portio.h"

/* Небольшая задержка: четыре чтения ALTSTATUS (~100ns) */
static inline void io_delay(uint16_t ctrl_port)
{
    (void)inb(ctrl_port + IDE_ALTSTATUS);
    (void)inb(ctrl_port + IDE_ALTSTATUS);
    (void)inb(ctrl_port + IDE_ALTSTATUS);
    (void)inb(ctrl_port + IDE_ALTSTATUS);
}

/* Ждём BSY=0 */
static int wait_bsy_clear(uint16_t base_port, uint16_t ctrl_port, uint32_t timeout)
{
    for (uint32_t i = 0; i < timeout; ++i)
    {
        uint8_t s = inb(base_port + IDE_STATUS);
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
        uint8_t s = inb(base_port + IDE_STATUS);
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
    uint8_t s = inb(base_port + IDE_STATUS);
    if (s & IDE_STATUS_ERR)
    {
        (void)inb(base_port + IDE_ERROR);
        return IDE_ERR_DEVICE;
    }
    return IDE_OK;
}

/* Выбрать устройство (LBA/CHS) и задержка */
static void select_device_and_delay(uint16_t base, uint16_t ctrl, uint8_t drive, int lba_flag, uint8_t head_high4)
{
    uint8_t value = (lba_flag ? 0xE0 : 0xA0) | ((drive & 1) << 4) | (head_high4 & 0x0F);
    outb(base + IDE_SELECT, value);
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
        aligned_words[i] = inw(base + IDE_DATA);
}

/* Записать один сектор (256 слов) из aligned_words */
static void write_sector_words_from(uint16_t base, const uint16_t aligned_words[256])
{
    for (int i = 0; i < 256; ++i)
        outw(base + IDE_DATA, aligned_words[i]);
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
    outb(disk->base_port + IDE_COMMAND, IDE_CMD_IDENTIFY);
    io_delay(disk->ctrl_port);

    uint8_t status = inb(disk->base_port + IDE_STATUS);
    if (status == 0)
        return IDE_ERR_DEVICE;

    /* Если ERR — возможно ATAPI */
    if (status & IDE_STATUS_ERR)
    {
        uint8_t cl = inb(disk->base_port + IDE_LCYL);
        uint8_t ch = inb(disk->base_port + IDE_HCYL);
        if ((cl == 0x14 && ch == 0xEB) || (cl == 0x69 && ch == 0x96))
        {
            outb(disk->base_port + IDE_COMMAND, IDE_CMD_IDENTIFY_PACKET);
            if (wait_bsy_clear(disk->base_port, disk->ctrl_port, IDE_TIMEOUT_LOOPS) != IDE_OK)
                return IDE_ERR_TIMEOUT;
            if (check_err_and_clear(disk->base_port) != IDE_OK)
                return IDE_ERR_DEVICE;
            if (wait_drq_or_err(disk->base_port, disk->ctrl_port, IDE_TIMEOUT_LOOPS) != IDE_OK)
                return IDE_ERR_TIMEOUT;
            for (int i = 0; i < 256; ++i)
                ident_buffer[i] = inw(disk->base_port + IDE_DATA);
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
        ident_buffer[i] = inw(disk->base_port + IDE_DATA);

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
    outb(base + IDE_NSECT, count);
    outb(base + IDE_SECTOR, (uint8_t)(lba & 0xFF));
    outb(base + IDE_LCYL, (uint8_t)((lba >> 8) & 0xFF));
    outb(base + IDE_HCYL, (uint8_t)((lba >> 16) & 0xFF));
}

/* Настройка регистров для LBA48 */
static void setup_lba48_regs(uint16_t base, uint16_t ctrl, uint64_t lba, uint16_t count, uint8_t drive)
{
    select_device_and_delay(base, ctrl, drive, 1, (uint8_t)((lba >> 24) & 0x0F));
    io_delay(ctrl);

    outb(base + IDE_NSECT, (uint8_t)((count >> 8) & 0xFF)); /* SECCOUNT1 */
    outb(base + IDE_SECTOR, (uint8_t)((lba >> 24) & 0xFF)); /* LBA3 */
    outb(base + IDE_LCYL, (uint8_t)((lba >> 32) & 0xFF));   /* LBA4 */
    outb(base + IDE_HCYL, (uint8_t)((lba >> 40) & 0xFF));   /* LBA5 */

    outb(base + IDE_NSECT, (uint8_t)(count & 0xFF));      /* SECCOUNT0 */
    outb(base + IDE_SECTOR, (uint8_t)(lba & 0xFF));       /* LBA0 */
    outb(base + IDE_LCYL, (uint8_t)((lba >> 8) & 0xFF));  /* LBA1 */
    outb(base + IDE_HCYL, (uint8_t)((lba >> 16) & 0xFF)); /* LBA2 */
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
                outb(disk->base_port + IDE_COMMAND, IDE_CMD_READ_SECTORS_EXT);
            }
            else
            {
                uint32_t cur_lba32 = (uint32_t)cur_lba;
                setup_lba28_regs(disk->base_port, disk->ctrl_port, cur_lba32, 1, disk->drive);
                outb(disk->base_port + IDE_COMMAND, IDE_CMD_READ_SECTORS);
            }

            int rc = wait_bsy_clear(disk->base_port, disk->ctrl_port, IDE_TIMEOUT_LOOPS);
            if (rc != IDE_OK)
                return rc;

            rc = wait_drq_or_err(disk->base_port, disk->ctrl_port, IDE_TIMEOUT_LOOPS);
            if (rc != IDE_OK)
                return rc;

            int bytes_per_sector = disk->sector_size;
            int words_per_sector = bytes_per_sector / 2;

            uint8_t *dest = user_buf + ((count - remaining) + s) * (size_t)bytes_per_sector;
            if (is_aligned_2(dest) && bytes_per_sector == 512)
            {
                uint16_t *wptr = (uint16_t *)dest;
                for (int i = 0; i < words_per_sector; ++i)
                    wptr[i] = inw(disk->base_port + IDE_DATA);
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
                        uint16_t w = inw(disk->base_port + IDE_DATA);
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
                outb(disk->base_port + IDE_COMMAND, IDE_CMD_WRITE_SECTORS_EXT);
                used_lba48 = 1;
            }
            else
            {
                uint32_t cur_lba32 = (uint32_t)cur_lba;
                setup_lba28_regs(disk->base_port, disk->ctrl_port, cur_lba32, 1, disk->drive);
                outb(disk->base_port + IDE_COMMAND, IDE_CMD_WRITE_SECTORS);
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
                    outw(disk->base_port + IDE_DATA, wptr[i]);
            }
            else
            {
                if (bytes_per_sector == 512)
                {
                    memcpy(tmp_sector_words, src, 512);
                    for (int i = 0; i < words_per_sector; ++i)
                        outw(disk->base_port + IDE_DATA, tmp_sector_words[i]);
                }
                else
                {
                    for (int i = 0; i < words_per_sector; ++i)
                    {
                        uint16_t w = (uint16_t)src[2 * i] | ((uint16_t)src[2 * i + 1] << 8);
                        outw(disk->base_port + IDE_DATA, w);
                    }
                }
            }

            if (wait_bsy_clear(disk->base_port, disk->ctrl_port, IDE_TIMEOUT_LOOPS) != IDE_OK)
                return IDE_ERR_TIMEOUT;

            if (check_err_and_clear(disk->base_port) != IDE_OK)
                return IDE_ERR_DEVICE;
        }

        user_buf += (size_t)chunk * disk->sector_size;
        remaining -= chunk;
        lba += chunk;
    }

    /* Flush cache */
    if (used_lba48)
        outb(disk->base_port + IDE_COMMAND, IDE_CMD_CACHE_FLUSH_EXT);
    else
        outb(disk->base_port + IDE_COMMAND, IDE_CMD_CACHE_FLUSH);

    if (wait_bsy_clear(disk->base_port, disk->ctrl_port, IDE_TIMEOUT_LOOPS) != IDE_OK)
        return IDE_ERR_TIMEOUT;
    if (check_err_and_clear(disk->base_port) != IDE_OK)
        return IDE_ERR_DEVICE;

    return IDE_OK;
}
