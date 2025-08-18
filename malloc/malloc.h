#ifndef KERNEL_MALLOC_H
#define KERNEL_MALLOC_H

#include <stddef.h>
#include "../libc/string.h"

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

void malloc_init(void *heap_start, size_t heap_size);
void *malloc(size_t size);
void free(void *ptr);
void *realloc(void *ptr, size_t new_size);
void print_kmalloc_stats(void);
void get_kmalloc_stats(kmalloc_stats_t *st);

#endif // KERNEL_MALLOC_H