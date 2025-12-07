#include "ramdisk.h"

static uint8_t ramdisk[RAMDISK_SIZE];

uint8_t *ramdisk_base(void)
{
    return ramdisk;
}
