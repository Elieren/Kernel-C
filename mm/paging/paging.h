#ifndef MM_PAGING_H
#define MM_PAGING_H

#include <stdint.h>
#include <stddef.h>

/* ─── Пул таблиц страниц ────────────────────────────────────────── */
uint64_t *pool_alloc(void);
void pool_free(uint64_t *pt);
void pool_reset(void);

/* ─── Public API ────────────────────────────────────────────────── */
void paging_init(uint64_t total_phys_mem);
void paging_switch(uint64_t *pml4);
uint64_t *paging_create_user_task(void *user_mem, size_t user_size);
void paging_destroy_user_task(uint64_t *pml4);
int paging_map_user_region(uint64_t *pml4, void *addr, size_t size);
void paging_unmap_user_region(uint64_t *pml4, void *addr, size_t size);

#endif