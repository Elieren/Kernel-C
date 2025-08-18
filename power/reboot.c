#include "reboot.h"
#include "../portio/portio.h"
#include <stdint.h>

void reboot_system(void)
{
    // Сбрасываем через Keyboard Controller (порт 0x64)
    // 0xFE — Pulse CPU Reset
    outb(0x64, 0xFE);

    // Если KBC не сработал — зависаем в цикле, QEMU обычно перезагружает
    while (1)
        asm volatile("hlt");
}
