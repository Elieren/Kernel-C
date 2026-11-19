#ifndef MM_PAGING_H
#define MM_PAGING_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

/* ─── Пул таблиц страниц ────────────────────────────────────────── */
uint64_t *pool_alloc(void);
void pool_free(uint64_t *pt);
void pool_reset(void);

/* ─── Public API ────────────────────────────────────────────────── */
void paging_init(uint64_t total_phys_mem);

/* ─── NX (No-Execute) ───────────────────────────────────────────── */
bool paging_nx_supported(void);
bool paging_nx_enabled(void);
bool paging_nx_usable(void);

/* Резервирует MMIO-диапазон ядра (LAPIC, framebuffer): user-таблицы строятся
 * только по RAM, без маппинга обращение к MMIO из user-задачи даст #PF. */
void paging_reserve_kernel_mmio(uint64_t phys, size_t size);
void paging_switch(uint64_t *pml4);
uint64_t *paging_create_user_task(void *user_mem, size_t user_size);
void paging_destroy_user_task(uint64_t *pml4);
int paging_map_user_region(uint64_t *pml4, void *addr, size_t size);
/* User-страницы всегда NX; регион — целые страницы из malloc_pages() */
int paging_map_user_region_nx(uint64_t *pml4, void *addr, size_t size);

void paging_unmap_user_region(uint64_t *pml4, void *addr, size_t size);

/* Права страниц образа (W^X) */
#define USER_PERM_W 0x1u
#define USER_PERM_X 0x2u

/* [offset, offset+size] от начала образа; чтение всегда разрешено */
typedef struct
{
    size_t offset;
    size_t size;
    uint32_t perm;
} user_region_t;

/* Выставляет права страницам образа по регионам; страница вне регионов — RW+NX.
 * 0, если страница получилась бы W+X. Без NX в CPU запрет исполнения невозможен. */
int paging_protect_user_image(uint64_t *pml4, void *image, size_t size,
                              const user_region_t *regions, size_t nregions);

#endif