#include "paging.h"
#include <stdint.h>
#include <string.h>

#define POOL_TABLES 512

typedef struct
{
    uint64_t e[512];
} __attribute__((aligned(4096))) pt_page_t;

static pt_page_t pool[POOL_TABLES];
static uint8_t pool_used[POOL_TABLES];

uint64_t *pool_alloc(void)
{
    for (int i = 0; i < POOL_TABLES; i++)
    {
        if (!pool_used[i])
        {
            pool_used[i] = 1;
            memset(&pool[i], 0, sizeof(pool[i]));
            return pool[i].e;
        }
    }
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
    if (idx >= 0 && idx < POOL_TABLES)
        pool_used[idx] = 0;
}

void pool_reset(void)
{
    memset(pool_used, 0, sizeof(pool_used));
}