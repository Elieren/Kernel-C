#ifndef BOOT_BOOTINFO_H
#define BOOT_BOOTINFO_H

#include <stdint.h>
#include <stdbool.h>
#include "arch/x86_64/boot/mb2/mb2.h"

// Полная информация о загрузке
typedef struct
{
    framebuffer_info_t fb; /* информация о фреймбуфере */
    uint64_t total_memory; /* общий размер памяти */
#if defined(__x86_64__) || defined(__i386__)
    uint64_t rsdp_addr; /* адрес RSDP (ACPI) */
#endif
} boot_info_t;

// Функция, которую реализует каждая архитектура
boot_info_t *arch_parse_boot_info(uint64_t boot_addr);

// Глобальная функция для получения информации о загрузке
boot_info_t *get_boot_info(void);

#define FB_TYPE_RGB 1

/* Настоящий линейный фреймбуфер: не EGA-текст 0xB8000, глубина — как умеет драйвер */
static inline bool fb_is_linear(const framebuffer_info_t *fb)
{
    return fb->fb_type == FB_TYPE_RGB && fb->addr && fb->pitch &&
           fb->width >= 320 && fb->height >= 200 &&
           (fb->bpp == 24 || fb->bpp == 32);
}

#endif /* BOOT_BOOTINFO_H */