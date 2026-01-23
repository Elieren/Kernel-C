#include "reboot.h"
#include <asm/io.h>

static inline void wait_kbc(void)
{
    // Ждём, пока контроллер освободится
    while (io_read8(0x64) & 0x02)
        ;
}

void reboot_system(void)
{
    // Попытка сброса через KBC
    wait_kbc();
    io_write8(0x64, 0xFE);

    // Если KBC не сработал, пробуем triple fault:
    asm volatile(
        "lidt (0) \n\t" // Загружаем нулевой IDT
        "int $3"        // Генерируем исключение -> triple fault -> reset
    );

    // Если triple fault не сработал (редкий случай)
    while (1)
        asm volatile("hlt");
}
