#ifndef USER_MALLOC_H
#define USER_MALLOC_H

#include <stdint.h>
#include <stddef.h>
#include <string.h>

typedef struct umalloc_stats
{
    size_t total_managed;
    size_t used_payload;
    size_t free_payload;
    size_t largest_free;
    size_t num_blocks;
    size_t num_used;
    size_t num_free;
} umalloc_stats_t;

/* Инициализация user heap: start — начало, size — размер */
void user_malloc_init(void);

/* Выделение/освобождение */
void *user_malloc(size_t size);
void user_free(void *ptr);
void *user_realloc(void *ptr, size_t new_size);

/* Статистика */
void get_usermalloc_stats(umalloc_stats_t *st);

#endif // USER_MALLOC_H
