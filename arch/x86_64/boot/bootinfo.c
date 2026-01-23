#include <boot/bootinfo.h>
#include "arch/x86_64/boot/mb2/mb2.h"
#include <string.h>

static boot_info_t g_boot_info;
static bool g_initialized = false;

boot_info_t *arch_parse_boot_info(uint64_t mb2_addr)
{
    if (g_initialized)
    {
        return &g_boot_info;
    }

    // Парсим Multiboot2
    mb2_parse(mb2_addr);

    // Получаем данные из MB2
    framebuffer_info_t *mb2_fb = get_framebuffer_info();

    // Копируем framebuffer info
    g_boot_info.fb.addr = mb2_fb->addr;
    g_boot_info.fb.pitch = mb2_fb->pitch;
    g_boot_info.fb.width = mb2_fb->width;
    g_boot_info.fb.height = mb2_fb->height;
    g_boot_info.fb.bpp = mb2_fb->bpp;

    // Получаем остальную информацию
    g_boot_info.rsdp_addr = get_rsdp_address();
    g_boot_info.total_memory = get_total_memory();

    g_initialized = true;
    return &g_boot_info;
}

boot_info_t *get_boot_info(void)
{
    return g_initialized ? &g_boot_info : NULL;
}