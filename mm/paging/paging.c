#include "paging.h"
#include "mm/malloc/malloc.h"
#include <stdint.h>
#include <stddef.h>
#include <string.h>

/* ─── Размерные константы ───────────────────────────────────────── */
#define PAGE_SIZE 4096ULL
#define TWO_MB (2ULL << 20)
#define PTE_ADDR 0x000FFFFFFFFFF000ULL

/* ─── Флаги PTE ─────────────────────────────────────────────────── */
#define PTE_P (1ULL << 0)  /* Present                         */
#define PTE_W (1ULL << 1)  /* Writable                        */
#define PTE_U (1ULL << 2)  /* User-accessible (ring-3 может)  */
#define PTE_PS (1ULL << 7) /* Page Size: 2MB large page       */

/* ─── Индексы уровней ───────────────────────────────────────────── */
#define PML4_IDX(va) (((uint64_t)(va) >> 39) & 0x1FF)
#define PDPT_IDX(va) (((uint64_t)(va) >> 30) & 0x1FF)
#define PD_IDX(va) (((uint64_t)(va) >> 21) & 0x1FF)
#define PT_IDX(va) (((uint64_t)(va) >> 12) & 0x1FF)

/* Выравнивания */
#define ALIGN_DOWN_2MB(x) ((uint64_t)(x) & ~(TWO_MB - 1))
#define ALIGN_UP_2MB(x) (((uint64_t)(x) + TWO_MB - 1) & ~(TWO_MB - 1))
#define ALIGN_DOWN_4KB(x) ((uint64_t)(x) & ~(PAGE_SIZE - 1))
#define ALIGN_UP_4KB(x) (((uint64_t)(x) + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1))

/* ─── Пул таблиц страниц ────────────────────────────────────────── */
#define POOL_TABLES 512

typedef struct
{
    uint64_t e[512];
} __attribute__((aligned(4096))) pt_page_t;

static pt_page_t pool[POOL_TABLES];
static uint8_t pool_used[POOL_TABLES];

static uint64_t *pool_alloc(void)
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

static void pool_free(uint64_t *pt)
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

/* ─── CPU-хелперы ───────────────────────────────────────────────── */
static inline uint64_t read_cr3(void)
{
    uint64_t v;
    __asm__ volatile("mov %%cr3, %0" : "=r"(v));
    return v;
}

static inline void write_cr3(uint64_t v)
{
    __asm__ volatile("mov %0, %%cr3" ::"r"(v) : "memory");
}

static inline void invlpg(uint64_t va)
{
    __asm__ volatile("invlpg (%0)" ::"r"(va) : "memory");
}

/* ─── Глобальное состояние ──────────────────────────────────────── */
static uint64_t kernel_cr3 = 0;
static uint64_t phys_mem_total = 0;

/* ─── Внутренние утилиты ────────────────────────────────────────── */
static uint64_t *get_or_create_supervisor(uint64_t *entry)
{
    if (!(*entry & PTE_P))
    {
        uint64_t *tbl = pool_alloc();
        if (!tbl)
            return NULL;
        *entry = (uint64_t)(uintptr_t)tbl | PTE_P | PTE_W;
        return tbl;
    }
    if (*entry & PTE_PS)
        return NULL;
    return (uint64_t *)(*entry & PTE_ADDR);
}

static uint64_t *ensure_user_parent(uint64_t *entry)
{
    if (!(*entry & PTE_P))
    {
        uint64_t *tbl = pool_alloc();
        if (!tbl)
            return NULL;
        *entry = (uint64_t)(uintptr_t)tbl | PTE_P | PTE_W | PTE_U;
        return tbl;
    }
    *entry |= PTE_U;
    if (*entry & PTE_PS)
        return NULL;
    return (uint64_t *)(*entry & PTE_ADDR);
}

static uint64_t *split_large_2mb(uint64_t *pd, int pd_i)
{
    uint64_t large = pd[pd_i];
    if (!(large & PTE_P) || !(large & PTE_PS))
        return NULL;

    uint64_t base_pa = large & ~(TWO_MB - 1);
    uint64_t *pt = pool_alloc();
    if (!pt)
        return NULL;

    for (int i = 0; i < 512; i++)
        pt[i] = (base_pa + (uint64_t)i * PAGE_SIZE) | PTE_P | PTE_W; /* U=0 */

    pd[pd_i] = (uint64_t)(uintptr_t)pt | PTE_P | PTE_W | PTE_U;
    return pt;
}

static uint64_t *get_or_create_pt_for_user(uint64_t *pml4, uint64_t va)
{
    uint64_t *pdpt = ensure_user_parent(&pml4[PML4_IDX(va)]);
    if (!pdpt)
        return NULL;

    uint64_t *pdpte = &pdpt[PDPT_IDX(va)];
    if (*pdpte & PTE_PS)
        return NULL;
    uint64_t *pd = ensure_user_parent(pdpte);
    if (!pd)
        return NULL;

    uint64_t *pde = &pd[PD_IDX(va)];

    if (!(*pde & PTE_P))
    {
        uint64_t *pt = pool_alloc();
        if (!pt)
            return NULL;
        uint64_t base = ALIGN_DOWN_2MB(va);
        for (int i = 0; i < 512; i++)
            pt[i] = (base + (uint64_t)i * PAGE_SIZE) | PTE_P | PTE_W; /* U=0 */
        *pde = (uint64_t)(uintptr_t)pt | PTE_P | PTE_W | PTE_U;
        return pt;
    }

    if (*pde & PTE_PS)
        return split_large_2mb(pd, (int)PD_IDX(va));

    return (uint64_t *)(*pde & PTE_ADDR);
}

/* ═══════════════════════════════════════════════════════════════════
 *  PUBLIC API
 * ═══════════════════════════════════════════════════════════════════ */

void paging_init(uint64_t total_phys_mem)
{
    kernel_cr3 = read_cr3();
    phys_mem_total = total_phys_mem;
    memset(pool_used, 0, sizeof(pool_used));
}

uint64_t *paging_get_kernel_cr3(void)
{
    return (uint64_t *)(kernel_cr3 & PTE_ADDR);
}

void paging_switch(uint64_t *pml4)
{
    if (!pml4)
    {
        write_cr3(kernel_cr3);
        return;
    }
    write_cr3((uint64_t)(uintptr_t)pml4);
}

uint64_t *paging_create_user_task(void *user_mem, size_t user_size)
{
    if (!user_mem || user_size == 0 || phys_mem_total == 0)
        return NULL;

    uint64_t user_start = ALIGN_DOWN_4KB((uint64_t)(uintptr_t)user_mem);
    uint64_t user_end = ALIGN_UP_4KB((uint64_t)(uintptr_t)user_mem + user_size);

    uint64_t *pml4 = pool_alloc();
    if (!pml4)
        return NULL;

    uint64_t map_end = ALIGN_UP_2MB(phys_mem_total);

    for (uint64_t addr = 0; addr < map_end; addr += TWO_MB)
    {
        uint64_t range_end = addr + TWO_MB;
        int overlaps = (addr < user_end && range_end > user_start);

        if (!overlaps)
        {
            /* ── Чистый ядровой блок: 2MB large page, U/S=0 ── */
            uint64_t *pdpt = get_or_create_supervisor(&pml4[PML4_IDX(addr)]);
            if (!pdpt)
                goto fail;
            uint64_t *pd = get_or_create_supervisor(&pdpt[PDPT_IDX(addr)]);
            if (!pd)
                goto fail;

            uint64_t *pde = &pd[PD_IDX(addr)];
            if (!(*pde & PTE_P))
                *pde = addr | PTE_P | PTE_W | PTE_PS;
            /* U-бит не ставим → ring-3 #PF */
        }
        else
        {
            /* ── Смешанный блок: PT 4KB, user-страницы U/S=1 ── */
            uint64_t *pdpt = ensure_user_parent(&pml4[PML4_IDX(addr)]);
            if (!pdpt)
                goto fail;

            uint64_t *pdpte = &pdpt[PDPT_IDX(addr)];
            if (*pdpte & PTE_PS)
                goto fail;
            uint64_t *pd = ensure_user_parent(pdpte);
            if (!pd)
                goto fail;

            uint64_t *pt = pool_alloc();
            if (!pt)
                goto fail;

            for (int pt_i = 0; pt_i < 512; pt_i++)
            {
                uint64_t page_pa = addr + (uint64_t)pt_i * PAGE_SIZE;
                int is_user = (page_pa >= user_start && page_pa < user_end);
                pt[pt_i] = page_pa | PTE_P | PTE_W | (is_user ? PTE_U : 0);
            }

            pd[PD_IDX(addr)] = (uint64_t)(uintptr_t)pt | PTE_P | PTE_W | PTE_U;
        }
    }

    return pml4;

fail:
    paging_destroy_user_task(pml4);
    return NULL;
}

int paging_map_user_region(uint64_t *pml4, void *addr, size_t size)
{
    if (!pml4 || !addr || size == 0)
        return 0;

    uint64_t start = ALIGN_DOWN_4KB((uint64_t)(uintptr_t)addr);
    uint64_t end = ALIGN_UP_4KB((uint64_t)(uintptr_t)addr + size);

    for (uint64_t page = start; page < end; page += PAGE_SIZE)
    {
        if (page >= phys_mem_total)
            break; /* выше RAM — не маппим */

        uint64_t *pt = get_or_create_pt_for_user(pml4, page);
        if (!pt)
            return 0;

        pt[PT_IDX(page)] |= PTE_U;
        invlpg(page);
    }

    return 1;
}

void paging_unmap_user_region(uint64_t *pml4, void *addr, size_t size)
{
    if (!pml4 || !addr || size == 0)
        return;

    uint64_t start = ALIGN_DOWN_4KB((uint64_t)(uintptr_t)addr);
    uint64_t end = ALIGN_UP_4KB((uint64_t)(uintptr_t)addr + size);

    for (uint64_t page = start; page < end; page += PAGE_SIZE)
    {
        if (page >= phys_mem_total)
            break;

        uint64_t pml4e_v = pml4[PML4_IDX(page)];
        if (!(pml4e_v & PTE_P) || (pml4e_v & PTE_PS))
            continue;

        uint64_t *pdpt = (uint64_t *)(pml4e_v & PTE_ADDR);
        uint64_t pdpte_v = pdpt[PDPT_IDX(page)];
        if (!(pdpte_v & PTE_P) || (pdpte_v & PTE_PS))
            continue;

        uint64_t *pd = (uint64_t *)(pdpte_v & PTE_ADDR);
        uint64_t pde_v = pd[PD_IDX(page)];
        if (!(pde_v & PTE_P) || (pde_v & PTE_PS))
            continue;

        uint64_t *pt = (uint64_t *)(pde_v & PTE_ADDR);
        pt[PT_IDX(page)] &= ~PTE_U;
        invlpg(page);
    }
}

void paging_destroy_user_task(uint64_t *pml4)
{
    if (!pml4)
        return;

    for (int i4 = 0; i4 < 512; i4++)
    {
        if (!(pml4[i4] & PTE_P) || (pml4[i4] & PTE_PS))
            continue;
        uint64_t *pdpt = (uint64_t *)(pml4[i4] & PTE_ADDR);

        for (int i3 = 0; i3 < 512; i3++)
        {
            if (!(pdpt[i3] & PTE_P) || (pdpt[i3] & PTE_PS))
                continue;
            uint64_t *pd = (uint64_t *)(pdpt[i3] & PTE_ADDR);

            for (int i2 = 0; i2 < 512; i2++)
            {
                if (!(pd[i2] & PTE_P) || (pd[i2] & PTE_PS))
                    continue;
                pool_free((uint64_t *)(pd[i2] & PTE_ADDR));
            }
            pool_free(pd);
        }
        pool_free(pdpt);
    }
    pool_free(pml4);
}