#include "mb2.h"
#include <string.h>
#include <stdint.h>

/* Константы для работы с Multiboot2 */
#define MB2_TAG_HDR_SIZE 8 /* Размер заголовка тега Multiboot2 (type + size) */
#define MB2_TAG_ALIGN 8    /* Каждый тег выровнен по 8 байт */

#define MB2_TAG_TYPE_END 0           /* Тип тега "конец списка" */
#define MB2_TAG_TYPE_BASIC_MEMINFO 4 /* Basic Memory Information */
#define MB2_TAG_TYPE_FRAMEBUFFER 8   /* Тип тега "framebuffer" */
#define MB2_TAG_TYPE_ACPI_OLD 14     /* RSDP v1 (ACPI 1.0) */
#define MB2_TAG_TYPE_ACPI_NEW 15     /* RSDP v2 (ACPI 2.0+) */

/* Возможные минимальные размеры полезной нагрузки тега framebuffer */
#define MB2_FB_PAYLOAD_MINIMAL 8
#define MB2_FB_PAYLOAD_LEGACY 16
#define MB2_FB_PAYLOAD_MODERN 24

/* Минимальный размер RSDP v1 */
#define RSDP_V1_SIZE 20
/* Минимальный размер RSDP v2 */
#define RSDP_V2_SIZE 36

/* Константы для работы с RSDP */
#define RSDP_SIGNATURE "RSD PTR "
#define RSDP_REVISION_OFFSET 15
#define RSDP_LENGTH_OFFSET 20

/* Значения по умолчанию */
#define DEFAULT_FB_SIZE 0x200000 /* 2 MiB */

/* Глобальная структура с информацией о фреймбуфере */
static framebuffer_info_t fb_info;

/* Глобальная переменная для хранения физического адреса RSDP */
static uint64_t g_rsdp_phys_addr = 0;

/* Глобальная переменная для хранения размера памяти */
static uint64_t g_total_memory = 0;

/* Вспомогательная функция - выравнивает значение вверх до 8 байт */
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

/* Вычисление контрольной суммы */
static inline uint8_t calc_checksum(const uint8_t *data, size_t len)
{
    uint8_t sum = 0;
    for (size_t i = 0; i < len; i++)
        sum += data[i];
    return sum;
}

/* Проверка валидности RSDP по сигнатуре и контрольной сумме */
static int validate_rsdp(const uint8_t *rsdp_data, size_t size)
{
    /* Минимальный размер для RSDP v1 */
    if (size < RSDP_V1_SIZE)
        return 0;

    /* Проверяем сигнатуру "RSD PTR " */
    if (memcmp(rsdp_data, RSDP_SIGNATURE, 8) != 0)
        return 0;

    /* Проверяем контрольную сумму для первых 20 байт (RSDP v1) */
    if (calc_checksum(rsdp_data, RSDP_V1_SIZE) != 0)
        return 0;

    /* Если есть RSDP v2, проверяем расширенную контрольную сумму */
    if (size >= RSDP_V2_SIZE)
    {
        uint8_t revision = rsdp_data[RSDP_REVISION_OFFSET];
        if (revision >= 2)
        {
            /* Читаем длину из offset 20 (4 байта) */
            uint32_t length = read_u32(rsdp_data + RSDP_LENGTH_OFFSET);

            if (length >= RSDP_V2_SIZE && length <= size && length <= 4096)
            {
                if (calc_checksum(rsdp_data, length) != 0)
                    return 0;
            }
        }
    }

    return 1; /* RSDP валиден */
}

/* Обработка ACPI тегов */
static void process_acpi_tag(uint8_t *payload, size_t payload_len,
                             size_t min_size, int is_new_version)
{
    /* Используем только если RSDP v2 не был найден (для ACPI_OLD) */
    if ((!is_new_version && g_rsdp_phys_addr != 0) || payload_len < min_size)
        return;

    /* Валидируем RSDP */
    if (validate_rsdp(payload, payload_len))
    {
        g_rsdp_phys_addr = (uint64_t)(uintptr_t)payload;
    }
}

static void process_framebuffer_tag(uint8_t *payload, size_t payload_len)
{
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
    else if (payload_len >= MB2_FB_PAYLOAD_MINIMAL)
    {
        uint64_t addr64 = read_u64(payload + 0);
        fb_info.addr = addr64;
        fb_info.pitch = 0;
        fb_info.width = 0;
        fb_info.height = 0;
        fb_info.bpp = 0;
        fb_info.fb_type = 0;
    }
    /* Иначе данных недостаточно - игнорируем */
}

/* Функция вычисляет примерный размер фреймбуфера в байтах.
   Если высота неизвестна - возвращает минимум 2 МиБ. */
uint64_t fb_calc_size(const framebuffer_info_t *fb)
{
    if (!fb)
        return DEFAULT_FB_SIZE;

    /* Если неизвестен pitch (байт на строку) или высота - возвращаем 2 МиБ */
    if (fb->pitch == 0 || fb->height == 0)
        return DEFAULT_FB_SIZE;

    if (fb->height > UINT64_MAX / fb->pitch)
        return DEFAULT_FB_SIZE;

    /* Общий объём памяти, занимаемой изображением */
    uint64_t bytes = (uint64_t)fb->pitch * (uint64_t)fb->height;

    /* Округляем вверх до ближайшего кратного 2 МиБ */
    uint64_t rounded = (bytes + 0x1FFFFF) & ~((uint64_t)0x1FFFFF);
    if (rounded == 0)
        rounded = DEFAULT_FB_SIZE;

    return rounded;
}

/* Обработка тега информации о памяти */
static void process_meminfo_tag(uint8_t *payload, size_t payload_len)
{
    if (payload_len < 8) /* Нужно минимум 8 байт (mem_lower + mem_upper) */
        return;

    uint32_t mem_lower = read_u32(payload + 0); /* в KiB, обычно 640 */
    uint32_t mem_upper = read_u32(payload + 4); /* в KiB, остальная память */

    /* Общий размер в байтах: lower всегда малый, поэтому используем upper */
    g_total_memory = (uint64_t)mem_upper * 1024 + (uint64_t)mem_lower * 1024;
}

/* Основная функция разбора структуры Multiboot2 */
void mb2_parse(uint64_t mb2_addr)
{
    if (mb2_addr == 0)
        return;

    /* Обнуляем структуру с информацией о фреймбуфере */
    memset(&fb_info, 0, sizeof(fb_info));

    /* Сбрасываем адрес RSDP */
    g_rsdp_phys_addr = 0;

    /* Указатель на начало Multiboot2-заголовка */
    uint8_t *base = (uint8_t *)(uintptr_t)mb2_addr;

    /* Первые 8 байт: общий размер и резерв */
    uint32_t total_size = read_u32(base + 0);

    /* Проверяем корректность размера */
    if (total_size < MB2_TAG_HDR_SIZE)
        return;

    uint8_t *end = base + total_size;       /* конец всей структуры */
    uint8_t *ptr = base + MB2_TAG_HDR_SIZE; /* первый тег идёт сразу после заголовка */

    /* Проходим по всем тегам */
    while (ptr + MB2_TAG_HDR_SIZE <= end)
    {
        /* Читаем заголовок тега */
        uint32_t tag_type = read_u32(ptr);
        uint32_t tag_size = read_u32(ptr + 4);

        /* Проверяем корректность размера тега */
        if (tag_size < MB2_TAG_HDR_SIZE)
            break;

        /* Вычисляем смещение к следующему тегу, с выравниванием */
        size_t aligned_size = align_up8((size_t)tag_size);
        uint8_t *next = ptr + aligned_size;

        if (next > end || next <= ptr)
            break; /* повреждённая структура или переполнение - выходим */

        /* Полезная нагрузка идёт сразу после 8-байтного заголовка */
        uint8_t *payload = ptr + MB2_TAG_HDR_SIZE;
        size_t payload_len = (size_t)tag_size - MB2_TAG_HDR_SIZE;

        /* Обрабатываем тег по типу */
        switch (tag_type)
        {
        /* Тег конца списка - выходим */
        case MB2_TAG_TYPE_END:
            return;

        /* Тег с информацией о framebuffer */
        case MB2_TAG_TYPE_FRAMEBUFFER:
            process_framebuffer_tag(payload, payload_len);
            break;

        case MB2_TAG_TYPE_BASIC_MEMINFO:
            process_meminfo_tag(payload, payload_len);
            break;

        /* Тег ACPI v2.0+ (RSDP v2) - предпочтительный */
        case MB2_TAG_TYPE_ACPI_NEW:
            process_acpi_tag(payload, payload_len, RSDP_V2_SIZE, 1);
            break;

        /* Тег ACPI v1.0 (RSDP v1) - используется если v2 не найден */
        case MB2_TAG_TYPE_ACPI_OLD:
            process_acpi_tag(payload, payload_len, RSDP_V1_SIZE, 0);
            break;

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

/* Возвращает физический адрес RSDP (0 если не найден) */
uint64_t get_rsdp_address(void)
{
    return g_rsdp_phys_addr;
}

/* Возвращает общий размер ОЗУ в байтах */
uint64_t get_total_memory(void)
{
    return g_total_memory;
}