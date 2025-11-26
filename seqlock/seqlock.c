#include "seqlock.h"

/* Писатель: начало */
void seqlock_write_begin(seqlock_t *s)
{
    /* seq становится нечётным → запись началась */
    atomic_fetch_add_explicit(&s->seq, 1u, memory_order_acquire);
}

/* Писатель: конец */
void seqlock_write_end(seqlock_t *s)
{
    /* seq становится чётным → запись завершена */
    atomic_fetch_add_explicit(&s->seq, 1u, memory_order_release);
}

/* Чтение: начало цикла */
unsigned seqlock_read_begin(const seqlock_t *s)
{
    unsigned seq;

    for (;;)
    {
        seq = atomic_load_explicit(&((seqlock_t *)s)->seq, memory_order_acquire);

        /* Нечётный → писатель пишет, ждём */
        if ((seq & 1u) == 0u)
            return seq;

        asm volatile("pause");
    }
}

/* Проверка повторения */
int seqlock_read_retry(const seqlock_t *s, unsigned start_seq)
{
    unsigned end_seq =
        atomic_load_explicit(&((seqlock_t *)s)->seq, memory_order_acquire);

    /* Если seq изменился или стал нечётным — повтор */
    return (end_seq != start_seq) || (end_seq & 1u);
}
