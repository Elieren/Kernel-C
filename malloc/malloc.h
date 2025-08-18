#ifndef KERNEL_MALLOC_H
#define KERNEL_MALLOC_H

#include <stddef.h>
#include "../libc/string.h"

void malloc_init(void *heap_start, size_t heap_size);
void *malloc(size_t size);
void free(void *ptr);
void *realloc(void *ptr, size_t new_size);
void print_kmalloc_stats(void);

#endif // KERNEL_MALLOC_H