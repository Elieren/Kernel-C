#ifndef IDE_H
#define IDE_H

#include <stdint.h>

// Базовые порты первичного канала IDE
#define IDE_BASE_PRIMARY 0x1F0
#define IDE_CTRL_PRIMARY 0x3F6

// Базовые порты вторичного канала IDE
#define IDE_BASE_SECONDARY 0x170
#define IDE_CTRL_SECONDARY 0x376

// Смещения регистров (от base)
#define IDE_DATA 0x00    // Data Register (16 бит)
#define IDE_ERROR 0x01   // Error Register (чтение)
#define IDE_FEATURE 0x01 // Features (запись)
#define IDE_NSECT 0x02   // Sector Count
#define IDE_SECTOR 0x03  // Sector Number (LBA0)
#define IDE_LCYL 0x04    // Low Cylinder (LBA1)
#define IDE_HCYL 0x05    // High Cylinder (LBA2)
#define IDE_SELECT 0x06  // Drive/Head Select
#define IDE_STATUS 0x07  // Status Register (чтение)
#define IDE_COMMAND 0x07 // Command Register (запись)

// Контрольный порт (ALT STATUS / CONTROL) - используется по адресу ctrl (например 0x3F6)
#define IDE_ALTSTATUS 0x0 // относительное к ctrl, чтение возвращает ALT STATUS
#define IDE_CONTROL 0x0   // относительное к ctrl, запись в CONTROL

// Биты статуса
#define IDE_STATUS_BSY 0x80  // Busy
#define IDE_STATUS_DRDY 0x40 // Drive Ready
#define IDE_STATUS_DRQ 0x08  // Data Request
#define IDE_STATUS_ERR 0x01  // Error

// Команды
#define IDE_CMD_READ_SECTORS 0x20
#define IDE_CMD_WRITE_SECTORS 0x30
#define IDE_CMD_IDENTIFY 0xEC

typedef enum
{
    IDE_CHANNEL_PRIMARY = 0,
    IDE_CHANNEL_SECONDARY = 1
} ide_channel_t;

typedef struct
{
    uint16_t base_port;     // Базовый порт (0x1F0 или 0x170)
    uint16_t ctrl_port;     // Контрольный порт (0x3F6 или 0x376)
    uint8_t drive;          // Номер диска на канале (0 = master, 1 = slave)
    ide_channel_t channel;  // Канал
    uint64_t total_sectors; // Объём диска в секторах
    uint16_t sector_size;   // Размер сектора (обычно 512)
} ide_disk_t;

int ide_init(ide_disk_t *disk, ide_channel_t channel, uint8_t drive);
int ide_read_sectors(ide_disk_t *disk, uint64_t lba, uint32_t count, void *buffer);
int ide_write_sectors(ide_disk_t *disk, uint64_t lba, uint32_t count, const void *buffer);
int ide_identify(ide_disk_t *disk, uint16_t *ident_buffer);

#endif // IDE_H