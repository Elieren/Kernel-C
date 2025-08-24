#ifndef RAMDISK_H
#define RAMDISK_H

#include <stdint.h>
#include <stddef.h>

#define RAMDISK_SIZE (64 * 1024 * 1024) // 64 MiB

// Получить указатель на базу RAM-диска
uint8_t *ramdisk_base(void);

#endif // RAMDISK_H
