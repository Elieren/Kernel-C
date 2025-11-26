#include "spinlock.h"

/* Глобальный lock для fb — atomic_flag (специальный тип) */
static atomic_flag g_fb_lock = ATOMIC_FLAG_INIT;

void fb_lock_acquire(void)
{
    while (atomic_flag_test_and_set_explicit(&g_fb_lock, memory_order_acquire))
    {
        asm volatile("pause");
    }
}

void fb_lock_release(void)
{
    atomic_flag_clear_explicit(&g_fb_lock, memory_order_release);
}

/* Дополнительный API (на базе spinlock_t, использует atomic_bool) */
void spin_lock(spinlock_t *l)
{
    /* atomic_exchange устанавливает true и возвращает предыдущее значение.
       Если предыдущее было true — кто-то другой держит lock, продолжаем спинить. */
    while (atomic_exchange_explicit(&l->flag, true, memory_order_acquire))
    {
        asm volatile("pause");
    }
}

void spin_unlock(spinlock_t *l)
{
    atomic_store_explicit(&l->flag, false, memory_order_release);
}

int spin_trylock(spinlock_t *l)
{
    bool expected = false;
    /* Попытаемся сменить false -> true без блокировки */
    if (atomic_compare_exchange_strong_explicit(
            &l->flag, &expected, true,
            memory_order_acquire, /* успех */
            memory_order_relaxed /* неуспех */))
    {
        return 1; /* удалось захватить */
    }
    return 0; /* уже захвачен */
}

int spin_is_locked(spinlock_t *l)
{
    /* Просто читаем текущее состояние */
    return atomic_load_explicit(&l->flag, memory_order_acquire) ? 1 : 0;
}
