#include "paging.h"
#include <stdint.h>
#include <string.h>
#include "lib/sync/spinlock/spinlock.h"

#define POOL_TABLES 512

typedef struct
{
    uint64_t e[512];
} __attribute__((aligned(4096))) pt_page_t;

static pt_page_t pool[POOL_TABLES];
static uint8_t pool_used[POOL_TABLES];

/* Пул таблиц страниц общий: без лока два ядра выдали бы один кадр под разные
 * таблицы. Лок листовой — из-под него ничего не вызывается. */
static spinlock_t g_pool_lock = SPINLOCK_INIT;

uint64_t *pool_alloc(void)
{
    unsigned long flags = spin_lock_irqsave(&g_pool_lock);
    for (int i = 0; i < POOL_TABLES; i++)
    {
        if (!pool_used[i])
        {
            pool_used[i] = 1;
            memset(&pool[i], 0, sizeof(pool[i]));
            spin_unlock_irqrestore(&g_pool_lock, flags);
            return pool[i].e;
        }
    }
    spin_unlock_irqrestore(&g_pool_lock, flags);
    return NULL;
}

void pool_free(uint64_t *pt)
{
    if (!pt)
        return;
    uintptr_t base = (uintptr_t)pool;
    uintptr_t addr = (uintptr_t)pt;
    if (addr < base || addr >= base + sizeof(pool))
        return;
    int idx = (int)((addr - base) / sizeof(pool[0]));

    unsigned long flags = spin_lock_irqsave(&g_pool_lock);
    if (idx >= 0 && idx < POOL_TABLES)
        pool_used[idx] = 0;
    spin_unlock_irqrestore(&g_pool_lock, flags);
}

void pool_reset(void)
{
    unsigned long flags = spin_lock_irqsave(&g_pool_lock);
    memset(pool_used, 0, sizeof(pool_used));
    spin_unlock_irqrestore(&g_pool_lock, flags);
}