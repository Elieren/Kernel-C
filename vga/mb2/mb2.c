#include "mb2.h"
#include <string.h>
#include <stdint.h>

/* Константы для работы с Multiboot2 */
#define MB2_TAG_HDR_SIZE 8 /* Размер заголовка тега Multiboot2 (type + size) */
#define MB2_TAG_ALIGN 8    /* Каждый тег выровнен по 8 байт */

#define MB2_TAG_TYPE_END 0         /* Тип тега "конец списка" */
#define MB2_TAG_TYPE_FRAMEBUFFER 8 /* Тип тега "framebuffer" */

/* Возможные минимальные размеры полезной нагрузки тега framebuffer */
#define MB2_FB_PAYLOAD_MINIMAL 8
#define MB2_FB_PAYLOAD_LEGACY 16
#define MB2_FB_PAYLOAD_MODERN 24

/* Глобальная структура с информацией о фреймбуфере */
static framebuffer_info_t fb_info;

/* Вспомогательная функция — выравнивает значение вверх до 8 байт */
static inline size_t align_up8(size_t x)
{
    return (x + (MB2_TAG_ALIGN - 1)) & ~(MB2_TAG_ALIGN - 1);
}

/* Безопасное чтение 32-битного значения из памяти
   (используется memcpy, чтобы избежать проблем с невыравненными адресами) */
static inline uint32_t read_u32(const void *p)
{
    uint32_t v;
    memcpy(&v, p, sizeof(v));
    return v;
}

/* Безопасное чтение 64-битного значения */
static inline uint64_t read_u64(const void *p)
{
    uint64_t v;
    memcpy(&v, p, sizeof(v));
    return v;
}

/* Функция вычисляет примерный размер фреймбуфера в байтах.
   Если высота неизвестна — возвращает минимум 2 МиБ. */
uint64_t fb_calc_size(const framebuffer_info_t *fb)
{
    if (!fb || fb->addr == 0)
        return 0;

    /* Если неизвестен pitch (байт на строку) или высота — возвращаем 2 МиБ */
    if (fb->pitch == 0 || fb->height == 0)
        return 0x200000;

    /* Общий объём памяти, занимаемой изображением */
    uint64_t bytes = (uint64_t)fb->pitch * (uint64_t)fb->height;

    /* Округляем вверх до ближайшего кратного 2 МиБ */
    uint64_t rounded = (bytes + 0x1FFFFF) & ~((uint64_t)0x1FFFFF);
    if (rounded == 0)
        rounded = 0x200000;

    return rounded;
}

/* Основная функция разбора структуры Multiboot2 */
void mb2_parse(uint64_t mb2_addr)
{
    if (mb2_addr == 0)
        return;

    /* Обнуляем структуру с информацией о фреймбуфере */
    memset(&fb_info, 0, sizeof(fb_info));

    /* Указатель на начало Multiboot2-заголовка */
    uint8_t *base = (uint8_t *)(uintptr_t)mb2_addr;

    /* Первые 8 байт: общий размер и резерв */
    uint32_t total_size = read_u32(base + 0);
    uint32_t reserved = read_u32(base + 4);

    /* Проверяем корректность размера */
    if (total_size < MB2_TAG_HDR_SIZE)
        return;

    uint8_t *end = base + total_size;       /* конец всей структуры */
    uint8_t *ptr = base + MB2_TAG_HDR_SIZE; /* первый тег идёт сразу после заголовка */

    /* Проходим по всем тегам */
    while (ptr + MB2_TAG_HDR_SIZE <= end)
    {
        /* Читаем заголовок тега */
        mb2_tag_t tag;
        memcpy(&tag, ptr, sizeof(tag));

        /* Проверяем корректность размера тега */
        if (tag.size < MB2_TAG_HDR_SIZE)
            break;

        /* Вычисляем смещение к следующему тегу, с выравниванием */
        size_t aligned_size = align_up8((size_t)tag.size);
        uint8_t *next = ptr + aligned_size;
        if (next > end)
            break; /* повреждённая структура — выходим */

        /* Обрабатываем тег по типу */
        switch (tag.type)
        {
        /* Тег конца списка — выходим */
        case MB2_TAG_TYPE_END:
            return;

        /* Тег с информацией о framebuffer */
        case MB2_TAG_TYPE_FRAMEBUFFER:
        {
            /* Полезная нагрузка идёт сразу после 8-байтного заголовка */
            uint8_t *payload = ptr + MB2_TAG_HDR_SIZE;
            size_t payload_len = (size_t)tag.size - MB2_TAG_HDR_SIZE;

            /* Современный формат: 64-битный адрес + pitch + width + height + bpp + тип */
            if (payload_len >= MB2_FB_PAYLOAD_MODERN)
            {
                uint64_t addr64 = read_u64(payload + 0);
                uint32_t pitch = read_u32(payload + 8);
                uint32_t width = read_u32(payload + 12);
                uint32_t height = read_u32(payload + 16);

                uint8_t bpp = 0;
                uint8_t fbtype = 0;
                memcpy(&bpp, payload + 20, 1);
                memcpy(&fbtype, payload + 21, 1);

                fb_info.addr = addr64;
                fb_info.pitch = pitch;
                fb_info.width = width;
                fb_info.height = height;
                fb_info.bpp = bpp;
                fb_info.fb_type = fbtype;
            }

            /* Старый формат: 32-битный адрес + pitch + width + height */
            else if (payload_len >= MB2_FB_PAYLOAD_LEGACY)
            {
                uint32_t addr32 = read_u32(payload + 0);
                uint32_t pitch = read_u32(payload + 4);
                uint32_t width = read_u32(payload + 8);
                uint32_t height = read_u32(payload + 12);

                fb_info.addr = (uint64_t)addr32;
                fb_info.pitch = pitch;
                fb_info.width = width;
                fb_info.height = height;
                fb_info.bpp = 0;
                fb_info.fb_type = 0;
            }

            /* Минимальный вариант: адрес + pitch + width */
            else if (payload_len >= 12)
            {
                uint32_t addr32 = read_u32(payload + 0);
                uint32_t pitch = read_u32(payload + 4);
                uint32_t width = read_u32(payload + 8);

                fb_info.addr = (uint64_t)addr32;
                fb_info.pitch = pitch;
                fb_info.width = width;
                fb_info.height = 0;
                fb_info.bpp = 0;
                fb_info.fb_type = 0;
            }

            /* Ещё более сокращённый вариант: только 64-битный адрес */
            else if (payload_len >= 8)
            {
                uint64_t addr64 = read_u64(payload + 0);
                fb_info.addr = addr64;
                fb_info.pitch = 0;
                fb_info.width = 0;
                fb_info.height = 0;
                fb_info.bpp = 0;
                fb_info.fb_type = 0;
            }

            /* Иначе данных недостаточно — игнорируем */
            break;
        }

        /* Все прочие теги игнорируем */
        default:
            break;
        }

        /* Переходим к следующему тегу */
        ptr = next;
    }
}

/* Возвращает указатель на заполненную структуру framebuffer_info */
framebuffer_info_t *get_framebuffer_info(void)
{
    return &fb_info;
}
