#ifndef SEQLOCK_H
#define SEQLOCK_H

#include <stdatomic.h>
#include <stdint.h>

/* Структура seqlock — атомарный sequence counter */
typedef struct
{
    atomic_uint seq;
} seqlock_t;

/* Инициализация */
#define SEQLOCK_INIT {ATOMIC_VAR_INIT(0u)}

/* Функции записи */
void seqlock_write_begin(seqlock_t *s);
void seqlock_write_end(seqlock_t *s);

/* Функции чтения */
unsigned seqlock_read_begin(const seqlock_t *s);
int seqlock_read_retry(const seqlock_t *s, unsigned start_seq);

#endif /* SEQLOCK_H */
