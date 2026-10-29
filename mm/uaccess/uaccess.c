#include "mm/uaccess/uaccess.h"
#include "mm/paging/paging.h"
#include "kernel/sched/multitask/multitask.h"

#define PTE_P 1ULL
#define PTE_U (1ULL << 2)
#define PTE_PS (1ULL << 7)
#define PTE_ADDR 0x000FFFFFFFFFF000ULL
#define PAGE_SZ 4096ULL

#define PML4_IDX(va) (((va) >> 39) & 0x1FF)
#define PDPT_IDX(va) (((va) >> 30) & 0x1FF)
#define PD_IDX(va) (((va) >> 21) & 0x1FF)
#define PT_IDX(va) (((va) >> 12) & 0x1FF)

/* Страница доступна из ring-3: Present и U/S=1 на каждом уровне,
   без huge-страниц (user-регионы в этом ядре всегда 4 KiB). */
static int page_is_user(const uint64_t *pml4, uint64_t va)
{
    uint64_t e = pml4[PML4_IDX(va)];
    if ((e & (PTE_P | PTE_U | PTE_PS)) != (PTE_P | PTE_U))
        return 0;

    const uint64_t *pdpt = (const uint64_t *)(uintptr_t)(e & PTE_ADDR);
    e = pdpt[PDPT_IDX(va)];
    if ((e & (PTE_P | PTE_U | PTE_PS)) != (PTE_P | PTE_U))
        return 0;

    const uint64_t *pd = (const uint64_t *)(uintptr_t)(e & PTE_ADDR);
    e = pd[PD_IDX(va)];
    if ((e & (PTE_P | PTE_U | PTE_PS)) != (PTE_P | PTE_U))
        return 0;

    const uint64_t *pt = (const uint64_t *)(uintptr_t)(e & PTE_ADDR);
    e = pt[PT_IDX(va)];
    return (e & (PTE_P | PTE_U)) == (PTE_P | PTE_U);
}

int user_range_ok(uint64_t *pml4, uint64_t uaddr, size_t len)
{
    if (!pml4)
        return 0;
    if (len == 0)
        return 1;
    if (uaddr >= USER_TOP || len > USER_TOP - uaddr)
        return 0;

    uint64_t end = uaddr + len;
    for (uint64_t page = uaddr & ~(PAGE_SZ - 1); page < end; page += PAGE_SZ)
    {
        if (!page_is_user(pml4, page))
            return 0;
    }
    return 1;
}

int user_range_ok_current(uint64_t uaddr, size_t len)
{
    task_t *cur = get_current_task();
    if (!cur)
        return 0;
    return user_range_ok(cur->page_table, uaddr, len);
}

int user_str_len(const char *s, size_t cap)
{
    task_t *cur = get_current_task();
    if (!s || cap == 0 || !cur || !cur->page_table)
        return -1;

    uint64_t addr = (uint64_t)(uintptr_t)s;
    size_t i = 0;

    while (i < cap)
    {
        uint64_t cur_addr = addr + i;
        if (cur_addr >= USER_TOP || !page_is_user(cur->page_table, cur_addr))
            return -1;

        /* проверена целая страница — сканируем до её конца */
        size_t in_page = (size_t)(PAGE_SZ - (cur_addr & (PAGE_SZ - 1)));
        if (in_page > cap - i)
            in_page = cap - i;

        const char *p = (const char *)(uintptr_t)cur_addr;
        for (size_t k = 0; k < in_page; k++)
        {
            if (p[k] == '\0')
                return (int)(i + k);
        }
        i += in_page;
    }
    return -1;
}

/* ── Учёт блоков задачи и безопасное снятие U/S ─────────────────────── */

/* Пересекает ли байтовый диапазон [base, base+len] страницу [page, page+4K]. */
static int range_hits_page(const void *base, size_t len, uint64_t page)
{
    if (!base || len == 0)
        return 0;

    uint64_t s = (uint64_t)(uintptr_t)base;
    uint64_t e = s + len;
    if (e < s)
        e = UINT64_MAX;

    return s < page + PAGE_SZ && e > page;
}

/* Страница всё ещё нужна задаче: образ, стек или любой записанный блок. */
static int page_in_use(const task_t *t, uint64_t page)
{
    if (range_hits_page(t->user_mem, t->user_mem_size, page))
        return 1;
    if (range_hits_page(t->user_stack, t->user_stack_size, page))
        return 1;

    for (int i = 0; i < USER_ALLOC_MAX; i++)
    {
        if (range_hits_page(t->user_allocs[i].ptr, t->user_allocs[i].size, page))
            return 1;
    }
    return 0;
}

user_alloc_t *user_alloc_find(task_t *t, const void *ptr)
{
    if (!t || !ptr)
        return NULL;

    for (int i = 0; i < USER_ALLOC_MAX; i++)
    {
        if (t->user_allocs[i].ptr == ptr)
            return &t->user_allocs[i];
    }
    return NULL;
}

user_alloc_t *user_alloc_slot(task_t *t)
{
    if (!t)
        return NULL;

    for (int i = 0; i < USER_ALLOC_MAX; i++)
    {
        if (t->user_allocs[i].ptr == NULL)
            return &t->user_allocs[i];
    }
    return NULL;
}

void user_pages_release(task_t *t, const void *addr, size_t size)
{
    if (!t || !t->page_table || !addr || size == 0)
        return;

    uint64_t start = (uint64_t)(uintptr_t)addr & ~(PAGE_SZ - 1);
    uint64_t end = ((uint64_t)(uintptr_t)addr + size + PAGE_SZ - 1) & ~(PAGE_SZ - 1);

    for (uint64_t page = start; page < end; page += PAGE_SZ)
    {
        if (!page_in_use(t, page))
            paging_unmap_user_region(t->page_table, (void *)(uintptr_t)page, PAGE_SZ);
    }
}
