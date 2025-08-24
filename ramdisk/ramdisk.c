#include "ramdisk.h"

static uint8_t ramdisk[RAMDISK_SIZE] = {0};

uint8_t *ramdisk_base(void)
{
    return ramdisk;
}
