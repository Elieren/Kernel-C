// malloc.c — malloc/realloc/free + статистика (print_kmalloc_stats)
#include "malloc.h"
#include <stdint.h>
#include <stddef.h>
#include "../vga/vga.h"
#include "../syscall/syscall.h"

/* Конфигурация */
#define ALIGN 8
#define MAGIC 0xB16B00B5U

/* Заголовок блока (payload идёт сразу после заголовка) */
typedef struct block_header
{
    uint32_t magic;
    size_t size; /* payload size в байтах */
    int free;    /* 1 если свободен, 0 если занят */
    struct block_header *prev;
    struct block_header *next;
} block_header_t;

#define MIN_SPLIT_SIZE (sizeof(block_header_t) + ALIGN)

/* Глобальные */
static block_header_t *heap_head = NULL;
static block_header_t *heap_tail = NULL;
static void *managed_heap_end = NULL;
static unsigned char *brk_ptr = NULL; /* текущий предел (bump pointer внутри области) */

/* Символы из link.ld */
extern char _heap_start;
extern char _heap_end;

/* Внешние функции (реализованы в других файлах вашего ядра) */
extern void *memcpy(void *dst, const void *src, size_t n);
extern void console_puts(const char *s);

static inline size_t align_up(size_t n)
{
    return (n + (ALIGN - 1)) & ~(ALIGN - 1);
}
static inline void *header_to_payload(block_header_t *h)
{
    return (void *)((char *)h + sizeof(block_header_t));
}
static inline block_header_t *payload_to_header(void *p)
{
    return (block_header_t *)((char *)p - sizeof(block_header_t));
}

/* Инициализация: передайте _heap_start и размер (в байтах) */
void malloc_init(void *heap_start, size_t heap_size)
{
    if (!heap_start || heap_size < sizeof(block_header_t))
        return;

    heap_head = (block_header_t *)heap_start;
    heap_head->magic = MAGIC;
    heap_head->size = heap_size - sizeof(block_header_t);
    heap_head->free = 1;
    heap_head->prev = heap_head->next = NULL;

    heap_tail = heap_head;
    managed_heap_end = (char *)heap_start + heap_size;
    brk_ptr = (unsigned char *)heap_start + heap_size; /* brk_ptr хранит верх резервируемой области */
}

/* Вспомогательная: выделить память у движка morecore (bump) — без привязки к page allocator.
   Возвращает pointer на область размера >= bytes (включая заголовок), или NULL при исчерпании.
   Мы выделяем сверху вниз: brk_ptr двигается вниз при выделении. */
static void *simple_morecore(size_t bytes)
{
    /* Выравниваем bytes вверх */
    size_t req = align_up(bytes);

    unsigned char *new_brk = (unsigned char *)brk_ptr - req;
    if ((void *)new_brk < (void *)&_heap_start)
    {
        /* исчерпали область */
        return NULL;
    }

    brk_ptr = new_brk;
    return (void *)brk_ptr;
}

/* find first-fit */
static block_header_t *find_fit(size_t size)
{
    block_header_t *cur = heap_head;
    while (cur)
    {
        if (cur->free && cur->size >= size)
            return cur;
        cur = cur->next;
    }
    return NULL;
}

/* split блока */
static void split_block(block_header_t *h, size_t req_size)
{
    if (!h)
        return;
    if (h->size < req_size + MIN_SPLIT_SIZE)
        return;

    char *new_hdr_addr = (char *)header_to_payload(h) + req_size;
    block_header_t *newh = (block_header_t *)new_hdr_addr;
    newh->magic = MAGIC;
    newh->free = 1;
    newh->size = h->size - req_size - sizeof(block_header_t);
    newh->prev = h;
    newh->next = h->next;
    if (newh->next)
        newh->next->prev = newh;
    h->next = newh;
    h->size = req_size;
    if (heap_tail == h)
        heap_tail = newh;
}

/* coalesce */
static void coalesce(block_header_t *h)
{
    if (!h)
        return;
    if (h->next && h->next->free)
    {
        block_header_t *n = h->next;
        h->size = h->size + sizeof(block_header_t) + n->size;
        h->next = n->next;
        if (n->next)
            n->next->prev = h;
        if (heap_tail == n)
            heap_tail = h;
    }
    if (h->prev && h->prev->free)
    {
        block_header_t *p = h->prev;
        p->size = p->size + sizeof(block_header_t) + h->size;
        p->next = h->next;
        if (h->next)
            h->next->prev = p;
        if (heap_tail == h)
            heap_tail = p;
        h = p;
    }
}

/* попытка расширить heap: создаём новый блок в свободной области сверху (через simple_morecore)
   запрашивая минимум bytes + sizeof(block_header_t) */
static int heap_expand(size_t bytes)
{
    size_t need = align_up(bytes + sizeof(block_header_t));
    void *p = simple_morecore(need);
    if (!p)
        return 0;

    block_header_t *h = (block_header_t *)p;
    h->magic = MAGIC;
    h->free = 1;
    h->size = need - sizeof(block_header_t);
    h->prev = heap_tail;
    h->next = NULL;
    if (heap_tail)
        heap_tail->next = h;
    heap_tail = h;
    if (!heap_head)
        heap_head = h;
    return 1;
}

/* malloc */
void *malloc(size_t size)
{
    if (size == 0)
        return NULL;
    size = align_up(size);

    block_header_t *fit = find_fit(size);
    while (!fit)
    {
        if (!heap_expand(size))
            break;
        fit = find_fit(size);
    }
    if (!fit)
        return NULL;
    split_block(fit, size);
    fit->free = 0;
    return header_to_payload(fit);
}

/* free */
void free(void *ptr)
{
    if (!ptr)
        return;

    block_header_t *h = payload_to_header(ptr);

    /* проверяем magic */
    if (h->magic != MAGIC)
        return; /* повреждённый или неверный указатель */

    /* проверка на многократное освобождение */
    if (h->free)
        return; /* уже свободен, ничего не делаем */

    h->free = 1;

    /* объединяем соседние свободные блоки */
    coalesce(h);
}

/* realloc */
void *realloc(void *ptr, size_t new_size)
{
    if (!ptr)
        return malloc(new_size);
    if (new_size == 0)
    {
        free(ptr);
        return NULL;
    }

    block_header_t *h = payload_to_header(ptr);
    if (h->magic != MAGIC)
        return NULL;

    new_size = align_up(new_size);
    if (new_size <= h->size)
    {
        split_block(h, new_size);
        return ptr;
    }

    /* Попытка расширить in-place за счёт следующего свободного блока(ов) */
    if (h->next && h->next->free)
    {
        size_t sum = h->size;
        block_header_t *cur = h->next;
        while (cur && cur->free && sum < new_size)
        {
            sum += sizeof(block_header_t) + cur->size;
            cur = cur->next;
        }
        if (sum >= new_size)
        {
            /* объединяем до cur_prev */
            block_header_t *to = h->next;
            while (to && to->free && h->size < new_size)
            {
                h->size = h->size + sizeof(block_header_t) + to->size;
                to = to->next;
            }
            h->next = to;
            if (to)
                to->prev = h;
            split_block(h, new_size);
            h->free = 0;
            return ptr;
        }
    }

    /* Нельзя in-place — выделяем новый, копируем и освобождаем старый */
    void *newp = malloc(new_size);
    if (!newp)
        return NULL;
    size_t copy = (h->size < new_size) ? h->size : new_size;
    memcpy(newp, ptr, copy);
    free(ptr);
    return newp;
}

/* ---- stats for kernel malloc ---- */

/* структура статистики */
typedef struct
{
    size_t total_managed; /* payloads + headers (в байтах) */
    size_t used_payload;
    size_t free_payload;
    size_t largest_free;
    size_t num_blocks;
    size_t num_used;
    size_t num_free;
} kmalloc_stats_t;

/* Обойти список блоков и собрать статистику.
   heap_head и block_header_t — доступны в этом файле */
void get_kmalloc_stats(kmalloc_stats_t *st)
{
    if (!st)
        return;
    st->total_managed = 0;
    st->used_payload = 0;
    st->free_payload = 0;
    st->largest_free = 0;
    st->num_blocks = st->num_used = st->num_free = 0;

    block_header_t *cur = heap_head;
    while (cur)
    {
        st->num_blocks++;
        st->total_managed += sizeof(block_header_t) + cur->size;
        if (cur->free)
        {
            st->num_free++;
            st->free_payload += cur->size;
            if (cur->size > st->largest_free)
                st->largest_free = cur->size;
        }
        else
        {
            st->num_used++;
            st->used_payload += cur->size;
        }
        cur = cur->next;
    }
}

static size_t kstrlen(const char *s)
{
    size_t i = 0;
    if (!s)
        return 0;
    while (s[i])
        ++i;
    return i;
}

/* Быстрая утилита: перевод unsigned -> строка десятичная (buf размером >= 32) */
static char *u32_to_dec(uint32_t v, char *buf)
{
    char tmp[32];
    int i = 0;
    if (v == 0)
    {
        buf[0] = '0';
        buf[1] = '\0';
        return buf;
    }
    while (v)
    {
        tmp[i++] = '0' + (v % 10);
        v /= 10;
    }
    for (int j = 0; j < i; ++j)
        buf[j] = tmp[i - 1 - j];
    buf[i] = '\0';
    return buf;
}

/* Печать статистики в человекочитаемом виде.
   Использует console_puts(const char *s) — объявлен вверху extern. */
void print_kmalloc_stats(void)
{
    kmalloc_stats_t s;
    get_kmalloc_stats(&s);

    char nbuf[64];
    const uint32_t base_x = 20; /* отступ по X (в символах) */
    uint32_t x = base_x;
    uint32_t y = 10; /* стартовая строка по Y — при желании измените */
    const uint8_t fg = WHITE;
    const uint8_t bg = BLACK;

    /* Line 1: "Heap total: <bytes> bytes (<mb> MiB)" */
    x = base_x;
    sys_print_str("Heap total: ", x, y, fg, bg);
    x += (uint32_t)kstrlen("Heap total: ");
    u32_to_dec((uint32_t)s.total_managed, nbuf);
    sys_print_str(nbuf, x, y, fg, bg);
    x += (uint32_t)kstrlen(nbuf);
    sys_print_str(" bytes (", x, y, fg, bg);
    x += (uint32_t)kstrlen(" bytes (");
    uint32_t mb = (uint32_t)(s.total_managed / (1024 * 1024));
    u32_to_dec(mb, nbuf);
    sys_print_str(nbuf, x, y, fg, bg);
    x += (uint32_t)kstrlen(nbuf);
    sys_print_str(" MiB)", x, y, fg, bg);

    /* next line */
    ++y;

    /* Line 2: "Used: <bytes>" */
    x = base_x;
    sys_print_str("Used: ", x, y, fg, bg);
    x += (uint32_t)kstrlen("Used: ");
    u32_to_dec((uint32_t)s.used_payload, nbuf);
    sys_print_str(nbuf, x, y, fg, bg);

    ++y;

    /* Line 3: "Free: <bytes>" */
    x = base_x;
    sys_print_str("Free: ", x, y, fg, bg);
    x += (uint32_t)kstrlen("Free: ");
    u32_to_dec((uint32_t)s.free_payload, nbuf);
    sys_print_str(nbuf, x, y, fg, bg);

    ++y;

    /* Line 4: "Largest free: <bytes>" */
    x = base_x;
    sys_print_str("Largest free: ", x, y, fg, bg);
    x += (uint32_t)kstrlen("Largest free: ");
    u32_to_dec((uint32_t)s.largest_free, nbuf);
    sys_print_str(nbuf, x, y, fg, bg);

    ++y;

    /* Line 5: "Blocks: <num> (used=<u>, free=<f>)" */
    x = base_x;
    sys_print_str("Blocks: ", x, y, fg, bg);
    x += (uint32_t)kstrlen("Blocks: ");
    u32_to_dec((uint32_t)s.num_blocks, nbuf);
    sys_print_str(nbuf, x, y, fg, bg);
    x += (uint32_t)kstrlen(nbuf);
    sys_print_str(" (used=", x, y, fg, bg);
    x += (uint32_t)kstrlen(" (used=");
    u32_to_dec((uint32_t)s.num_used, nbuf);
    sys_print_str(nbuf, x, y, fg, bg);
    x += (uint32_t)kstrlen(nbuf);
    sys_print_str(", free=", x, y, fg, bg);
    x += (uint32_t)kstrlen(", free=");
    u32_to_dec((uint32_t)s.num_free, nbuf);
    sys_print_str(nbuf, x, y, fg, bg);
    sys_print_str(")", x + (uint32_t)kstrlen(nbuf), y, fg, bg);

    /* закончено — следующая полезная строка будет на y+1 */
}