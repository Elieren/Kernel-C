#ifndef IDE_H
#define IDE_H

#include <stdint.h>
#include <stddef.h>
#include <string.h>

/* Базовые порты (compatibility mode) */
#define IDE_BASE_PRIMARY 0x1F0
#define IDE_CTRL_PRIMARY 0x3F6
#define IDE_BASE_SECONDARY 0x170
#define IDE_CTRL_SECONDARY 0x376

/* Смещения регистров относительно base/ctrl */
#define IDE_DATA 0x00    /* word I/O */
#define IDE_ERROR 0x01   /* чтение */
#define IDE_FEATURE 0x01 /* запись */
#define IDE_NSECT 0x02
#define IDE_SECTOR 0x03  /* LBA0 */
#define IDE_LCYL 0x04    /* LBA1 */
#define IDE_HCYL 0x05    /* LBA2 */
#define IDE_SELECT 0x06  /* Drive/Head */
#define IDE_STATUS 0x07  /* чтение */
#define IDE_COMMAND 0x07 /* запись */

/* ctrl-порт */
#define IDE_ALTSTATUS 0x0 /* чтение */
#define IDE_CONTROL 0x0   /* запись */

/* Статус-биты */
#define IDE_STATUS_BSY 0x80
#define IDE_STATUS_DRDY 0x40
#define IDE_STATUS_DRQ 0x08
#define IDE_STATUS_ERR 0x01
#define IDE_STATUS_DF 0x20

/* Команды */
#define IDE_CMD_READ_SECTORS 0x20
#define IDE_CMD_WRITE_SECTORS 0x30
#define IDE_CMD_IDENTIFY 0xEC
#define IDE_CMD_IDENTIFY_PACKET 0xA1
#define IDE_CMD_READ_SECTORS_EXT 0x24
#define IDE_CMD_WRITE_SECTORS_EXT 0x34
#define IDE_CMD_CACHE_FLUSH 0xE7
#define IDE_CMD_CACHE_FLUSH_EXT 0xEA

/* Таймаут (итерации опроса) */
#define IDE_TIMEOUT_LOOPS 100000U

/* Коды возврата */
#define IDE_OK 0
#define IDE_ERR_TIMEOUT -1
#define IDE_ERR_DEVICE -2
#define IDE_ERR_INVALID -3

typedef enum
{
    IDE_CHANNEL_PRIMARY = 0,
    IDE_CHANNEL_SECONDARY = 1
} ide_channel_t;

typedef enum
{
    IDE_TYPE_NONE = 0,
    IDE_TYPE_ATA,
    IDE_TYPE_ATAPI
} ide_dev_type_t;

typedef struct
{
    uint16_t base_port; /* 0x1F0 / 0x170 */
    uint16_t ctrl_port; /* 0x3F6 / 0x376 */
    uint8_t drive;      /* 0 = master, 1 = slave */
    ide_channel_t channel;
    ide_dev_type_t type;    /* ATA / ATAPI / NONE */
    uint64_t total_sectors; /* число секторов (512B) */
    uint16_t sector_size;   /* размер сектора (обычно 512) */
    int supports_lba48;     /* поддержка LBA48 (bool) */
} ide_disk_t;

int ide_init(ide_disk_t *disk, ide_channel_t channel, uint8_t drive);
int ide_identify(ide_disk_t *disk, uint16_t ident_buffer[256]);
int ide_read_sectors(ide_disk_t *disk, uint64_t lba, uint32_t count, void *buffer);
int ide_write_sectors(ide_disk_t *disk, uint64_t lba, uint32_t count, const void *buffer);

#endif /* IDE_H */
