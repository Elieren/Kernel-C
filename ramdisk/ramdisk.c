#include "ramdisk.h"

static uint8_t ramdisk[RAMDISK_SIZE];

void ramdisk_init(void)
{
    for (size_t i = 0; i < RAMDISK_SIZE; i++)
        ramdisk[i] = 0;
}

uint8_t *ramdisk_base(void)
{
    return ramdisk;
}
