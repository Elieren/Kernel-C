#ifndef KERNEL_MALLOC_H
#define KERNEL_MALLOC_H

#include <stddef.h>
#include "lib/string/string.h"

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

#define HEAP_PAGE_SIZE 4096u

void malloc_init(void *heap_start, size_t heap_size);
void *malloc(size_t size);
void free(void *ptr);
void *realloc(void *ptr, size_t new_size);

/* Блок из npages страниц: выровнен по странице, обнулён, страницу не делит ни с кем.
 * Для памяти, отдаваемой в ring-3. Освобождается через free(). NULL при нехватке места. */
void *malloc_pages(size_t npages);
void get_kmalloc_stats(kmalloc_stats_t *st);
size_t malloc_usable_size(void *ptr);
size_t malloc_checked_size(void *ptr);
void malloc_panic_reset_lock(void); /* только из паники, после smp_halt_others() */

#endif // KERNEL_MALLOC_H