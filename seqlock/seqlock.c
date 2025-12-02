#include "seqlock.h"

/* Писатель: начало */
void seqlock_write_begin(seqlock_t *s)
{
    /* seq становится нечётным → запись началась */
    atomic_fetch_add_explicit(&s->seq, 1u, memory_order_relaxed);
}

/* Писатель: конец */
void seqlock_write_end(seqlock_t *s)
{
    /* seq становится чётным → запись завершена */
    /* memory_order_release гарантирует видимость ВСЕХ предыдущих записей */
    atomic_fetch_add_explicit(&s->seq, 1u, memory_order_release);
}

/* Чтение: начало цикла */
unsigned seqlock_read_begin(const seqlock_t *s)
{
    unsigned seq;

    for (;;)
    {
        /* memory_order_acquire синхронизируется с write_end */
        seq = atomic_load_explicit(&((seqlock_t *)s)->seq, memory_order_acquire);

        /* Чётный → можно читать */
        if ((seq & 1u) == 0u)
            return seq;

        /* Нечётный → писатель пишет, ждём */
        asm volatile("pause" ::: "memory");
    }
}

/* Проверка повторения */
int seqlock_read_retry(const seqlock_t *s, unsigned start_seq)
{
    /* КРИТИЧНО: барьер гарантирует, что все чтения данных
       завершены ДО проверки финального значения seq */
    atomic_thread_fence(memory_order_acquire);

    unsigned end_seq =
        atomic_load_explicit(&((seqlock_t *)s)->seq, memory_order_relaxed);

    return end_seq != start_seq;
}
