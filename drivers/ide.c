#include "ide.h"
#include <stddef.h>
#include <stdint.h>
#include "../portio/portio.h"

/* Небольшая задержка - чтение альтернативного статуса */
static inline void io_delay(uint16_t ctrl_port)
{
    // чтение ALTSTATUS порт тратит ~100ns; делаем несколько чтений для небольшой задержки
    (void)inb(ctrl_port + IDE_ALTSTATUS);
    (void)inb(ctrl_port + IDE_ALTSTATUS);
    (void)inb(ctrl_port + IDE_ALTSTATUS);
    (void)inb(ctrl_port + IDE_ALTSTATUS);
}

/* Ожидание, пока диск не будет занят (BSY == 0) */
static int wait_not_busy(uint16_t base_port, uint16_t ctrl_port, uint32_t timeout_loops)
{
    for (uint32_t i = 0; i < timeout_loops; i++)
    {
        uint8_t s = inb(base_port + IDE_STATUS);
        if (!(s & IDE_STATUS_BSY))
            return 0;
        // периодически делать небольшую задержку
        if ((i & 0xFF) == 0)
            io_delay(ctrl_port);
    }
    return -1; // таймаут
}

/* Ожидание готовности диска к передаче (DRDY=1, BSY=0) */
static int wait_drive_ready(uint16_t base_port, uint16_t ctrl_port, uint32_t timeout_loops)
{
    for (uint32_t i = 0; i < timeout_loops; i++)
    {
        uint8_t status = inb(base_port + IDE_STATUS);
        if ((status & IDE_STATUS_DRDY) && !(status & IDE_STATUS_BSY))
            return 0;
        if ((i & 0xFF) == 0)
            io_delay(ctrl_port);
    }
    return -1;
}

/* Проверка ошибки (ERR) и чтение регистра ошибок для сброса */
static int check_error(uint16_t base_port)
{
    uint8_t status = inb(base_port + IDE_STATUS);
    if (status & IDE_STATUS_ERR)
    {
        // чтение регистра ошибок (см. спецификацию)
        (void)inb(base_port + IDE_ERROR);
        return -1;
    }
    return 0;
}

/* Инициализация диска, отправка IDENTIFY и чтение параметров */
int ide_init(ide_disk_t *disk, ide_channel_t channel, uint8_t drive)
{
    if (!disk || drive > 1)
        return -1;

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

    disk->drive = drive;
    disk->channel = channel;
    disk->sector_size = 512;
    disk->total_sectors = 0;

    // Выбираем устройство (master/slave), устанавливаем бит 7 и бит 5 как 1 (0xA0)
    outb(disk->base_port + IDE_SELECT, 0xA0 | (drive << 4));

    // Ждём готовности
    if (wait_drive_ready(disk->base_port, disk->ctrl_port, 100000) != 0)
        return -1;

    // Отправляем IDENTIFY
    outb(disk->base_port + IDE_COMMAND, IDE_CMD_IDENTIFY);

    // Ждём, пока BSY сбросится
    if (wait_not_busy(disk->base_port, disk->ctrl_port, 100000) != 0)
        return -1;

    // Проверяем ошибку - если ошибка, возможно устройство отсутствует
    if (check_error(disk->base_port) != 0)
        return -1;

    // Читаем 256 слов идентификации
    uint16_t ident_buffer[256];
    for (int i = 0; i < 256; i++)
    {
        ident_buffer[i] = inw(disk->base_port + IDE_DATA);
    }

    // Если слово 0 == 0, устройства может не быть
    if (ident_buffer[0] == 0x0000)
        return -1;

    // Словa 60 и 61 - total number of user addressable sectors (LBA28)
    // Они уже прочитаны словами в порядке хоста (inw возвращает слово правильно для x86)
    disk->total_sectors = ((uint64_t)ident_buffer[61] << 16) | ident_buffer[60];

    return 0;
}

/* Чтение count секторов начиная с lba (буфер должен быть size >= 512 * count) */
int ide_read_sectors(ide_disk_t *disk, uint64_t lba, uint32_t count, void *buffer)
{
    if (!disk || !buffer || count == 0)
        return -1;

    uint16_t *buf16 = (uint16_t *)buffer;

    for (uint32_t s = 0; s < count; s++)
    {
        uint64_t cur_lba = lba + s;

        // Выбираем диск (LBA mode), устанавливаем верхние 4 бита LBA в селекторе
        outb(disk->base_port + IDE_SELECT,
             0xE0 | (disk->drive << 4) | (uint8_t)((cur_lba >> 24) & 0x0F));

        // Заполняем регистры LBA 0..2 и сектор count = 1
        outb(disk->base_port + IDE_NSECT, 1);
        outb(disk->base_port + IDE_SECTOR, (uint8_t)(cur_lba & 0xFF));
        outb(disk->base_port + IDE_LCYL, (uint8_t)((cur_lba >> 8) & 0xFF));
        outb(disk->base_port + IDE_HCYL, (uint8_t)((cur_lba >> 16) & 0xFF));

        // Команда чтения
        outb(disk->base_port + IDE_COMMAND, IDE_CMD_READ_SECTORS);

        // Ждём окончания BSY
        if (wait_not_busy(disk->base_port, disk->ctrl_port, 100000) != 0)
            return -1;

        // Ждём готовности данных (DRQ)
        uint8_t status;
        uint32_t loops = 0;
        for (;;)
        {
            status = inb(disk->base_port + IDE_STATUS);
            if (status & IDE_STATUS_ERR)
                return -1;
            if (!(status & IDE_STATUS_BSY) && (status & IDE_STATUS_DRQ))
                break;
            if (++loops > 100000)
                return -1;
        }

        // Читаем 256 слов = 512 байт
        for (int i = 0; i < 256; i++)
            buf16[i] = inw(disk->base_port + IDE_DATA);

        buf16 += 256;
    }

    return 0;
}

/* Запись count секторов начиная с lba (буфер должен быть size >= 512 * count) */
int ide_write_sectors(ide_disk_t *disk, uint64_t lba, uint32_t count, const void *buffer)
{
    if (!disk || !buffer || count == 0)
        return -1;

    const uint16_t *buf16 = (const uint16_t *)buffer;

    for (uint32_t s = 0; s < count; s++)
    {
        uint64_t cur_lba = lba + s;

        // Выбираем диск (LBA mode)
        outb(disk->base_port + IDE_SELECT,
             0xE0 | (disk->drive << 4) | (uint8_t)((cur_lba >> 24) & 0x0F));

        // Заполняем регистры LBA 0..2 и сектор count = 1
        outb(disk->base_port + IDE_NSECT, 1);
        outb(disk->base_port + IDE_SECTOR, (uint8_t)(cur_lba & 0xFF));
        outb(disk->base_port + IDE_LCYL, (uint8_t)((cur_lba >> 8) & 0xFF));
        outb(disk->base_port + IDE_HCYL, (uint8_t)((cur_lba >> 16) & 0xFF));

        // Команда записи
        outb(disk->base_port + IDE_COMMAND, IDE_CMD_WRITE_SECTORS);

        // Ждём окончания BSY
        if (wait_not_busy(disk->base_port, disk->ctrl_port, 100000) != 0)
            return -1;

        // Ждём готовности к приёму данных (DRQ)
        uint32_t loops = 0;
        for (;;)
        {
            uint8_t status = inb(disk->base_port + IDE_STATUS);
            if (status & IDE_STATUS_ERR)
                return -1;
            if (!(status & IDE_STATUS_BSY) && (status & IDE_STATUS_DRQ))
                break;
            if (++loops > 100000)
                return -1;
        }

        // Пишем 256 слов = 512 байт
        for (int i = 0; i < 256; i++)
            outw(disk->base_port + IDE_DATA, buf16[i]);

        // Ждём завершения (BSY)
        if (wait_not_busy(disk->base_port, disk->ctrl_port, 100000) != 0)
            return -1;

        // Проверяем ошибки
        if (check_error(disk->base_port) != 0)
            return -1;

        buf16 += 256;
    }

    return 0;
}

/* Запрос IDENTIFY (заполнение user-allocated буфера из 256 слов) */
int ide_identify(ide_disk_t *disk, uint16_t *ident_buffer)
{
    if (!disk || !ident_buffer)
        return -1;

    // Выбираем диск
    outb(disk->base_port + IDE_SELECT, 0xE0 | (disk->drive << 4));

    if (wait_drive_ready(disk->base_port, disk->ctrl_port, 100000) != 0)
        return -1;

    outb(disk->base_port + IDE_COMMAND, IDE_CMD_IDENTIFY);

    if (wait_not_busy(disk->base_port, disk->ctrl_port, 100000) != 0)
        return -1;

    if (check_error(disk->base_port) != 0)
        return -1;

    for (int i = 0; i < 256; i++)
        ident_buffer[i] = inw(disk->base_port + IDE_DATA);

    return 0;
}