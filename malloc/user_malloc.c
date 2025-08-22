// user_malloc.c — simple allocator для .user области
#include "user_malloc.h"
#include "../vga/vga.h"
#include "../syscall/syscall.h"

/* Конфигурация */
#define ALIGN 8
#define MAGIC 0xC0FFEE00U
#define MIN_SPLIT_SIZE (sizeof(user_block_t) + ALIGN)

/* Заголовок блока */
typedef struct user_block
{
    uint32_t magic;
    size_t size; /* payload size */
    int free;    /* 1 если свободен */
    struct user_block *prev;
    struct user_block *next;
} user_block_t;

/* Символы из link.ld (.user section) */
extern char _user_start;
extern char _user_end;

/* Глобальные */
static user_block_t *user_head = NULL;
static user_block_t *user_tail = NULL;
static unsigned char *user_brk = NULL; /* bump pointer */

static inline size_t align_up(size_t n)
{
    return (n + (ALIGN - 1)) & ~(ALIGN - 1);
}
static inline void *header_to_payload(user_block_t *h)
{
    return (void *)((char *)h + sizeof(user_block_t));
}
static inline user_block_t *payload_to_header(void *p)
{
    return (user_block_t *)((char *)p - sizeof(user_block_t));
}

/* Инициализация allocator */
void user_malloc_init(void)
{
    if (user_head)
        return; /* уже инициализировано */

    user_head = (user_block_t *)&_user_start;
    user_head->magic = MAGIC;
    user_head->size = (size_t)(&_user_end - &_user_start) - sizeof(user_block_t);
    user_head->free = 1;
    user_head->prev = user_head->next = NULL;
    user_tail = user_head;

    user_brk = (unsigned char *)&_user_end;
}

/* split блока */
static void split_block(user_block_t *h, size_t req_size)
{
    if (h->size < req_size + MIN_SPLIT_SIZE)
        return;

    char *new_hdr_addr = (char *)header_to_payload(h) + req_size;
    user_block_t *newh = (user_block_t *)new_hdr_addr;
    newh->magic = MAGIC;
    newh->free = 1;
    newh->size = h->size - req_size - sizeof(user_block_t);
    newh->prev = h;
    newh->next = h->next;
    if (newh->next)
        newh->next->prev = newh;
    h->next = newh;
    h->size = req_size;
    if (user_tail == h)
        user_tail = newh;
}

/* coalesce */
static void coalesce(user_block_t *h)
{
    if (!h)
        return;
    if (h->next && h->next->free)
    {
        user_block_t *n = h->next;
        h->size += sizeof(user_block_t) + n->size;
        h->next = n->next;
        if (n->next)
            n->next->prev = h;
        if (user_tail == n)
            user_tail = h;
    }
    if (h->prev && h->prev->free)
    {
        user_block_t *p = h->prev;
        p->size += sizeof(user_block_t) + h->size;
        p->next = h->next;
        if (h->next)
            h->next->prev = p;
        if (user_tail == h)
            user_tail = p;
        h = p;
    }
}

/* find first-fit */
static user_block_t *find_fit(size_t size)
{
    user_block_t *cur = user_head;
    while (cur)
    {
        if (cur->free && cur->size >= size)
            return cur;
        cur = cur->next;
    }
    return NULL;
}

/* user_malloc */
void *user_malloc(size_t size)
{
    if (!user_head)
        user_malloc_init();

    if (size == 0)
        return NULL;

    size = align_up(size);
    user_block_t *fit = find_fit(size);
    if (!fit)
        return NULL;

    split_block(fit, size);
    fit->free = 0;
    return header_to_payload(fit);
}

/* user_free */
void user_free(void *ptr)
{
    if (!ptr)
        return;

    user_block_t *h = payload_to_header(ptr);
    if (h->magic != MAGIC || h->free)
        return;

    h->free = 1;
    coalesce(h);
}

/* user_realloc */
void *user_realloc(void *ptr, size_t new_size)
{
    if (!ptr)
        return user_malloc(new_size);
    if (new_size == 0)
    {
        user_free(ptr);
        return NULL;
    }

    user_block_t *h = payload_to_header(ptr);
    if (h->magic != MAGIC)
        return NULL;

    new_size = align_up(new_size);
    if (new_size <= h->size)
    {
        split_block(h, new_size);
        return ptr;
    }

    /* Попытка расширить in-place */
    if (h->next && h->next->free && (h->size + sizeof(user_block_t) + h->next->size) >= new_size)
    {
        h->size += sizeof(user_block_t) + h->next->size;
        h->next = h->next->next;
        if (h->next)
            h->next->prev = h;
        split_block(h, new_size);
        h->free = 0;
        return ptr;
    }

    /* Выделяем новый блок и копируем */
    void *newp = user_malloc(new_size);
    if (!newp)
        return NULL;
    memcpy(newp, ptr, h->size);
    user_free(ptr);
    return newp;
}

/* Опционально: статистика */
void get_usermalloc_stats(umalloc_stats_t *st)
{
    if (!st)
        return;
    st->total_managed = 0;
    st->used_payload = 0;
    st->free_payload = 0;
    st->largest_free = 0;
    st->num_blocks = st->num_used = st->num_free = 0;

    user_block_t *cur = user_head;
    while (cur)
    {
        st->num_blocks++;
        st->total_managed += sizeof(user_block_t) + cur->size;
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
