#include "spinlock.h"
#include <asm/cpu.h>
#include <asm/smp.h>

/* Глобальный lock для fb - atomic_flag (специальный тип) */
static atomic_flag g_fb_lock = ATOMIC_FLAG_INIT;

void fb_lock_acquire(void)
{
    /* ПРИМЕЧАНИЕ: atomic_flag не поддерживает операцию чтения без модификации,
       поэтому TTAS оптимизация здесь невозможна.
       Для высокопроизводительных spinlock лучше использовать atomic_bool. */
    while (atomic_flag_test_and_set_explicit(&g_fb_lock, memory_order_acquire))
    {
        /* memory clobber для предотвращения переупорядочивания */
        cpu_relax();
    }
}

void fb_lock_release(void)
{
    atomic_flag_clear_explicit(&g_fb_lock, memory_order_release);
}

/* Дополнительный API (на базе spinlock_t, использует atomic_bool) */
void spin_lock(spinlock_t *l)
{
    /* TTAS (Test and Test-And-Set) оптимизация:
       Снижает cache coherency traffic в ~10-100 раз при конкуренции */
    while (atomic_exchange_explicit(&l->flag, true, memory_order_acquire))
    {
        /* Спиним на простом чтении (не инвалидирует cache line других ядер) */
        while (atomic_load_explicit(&l->flag, memory_order_relaxed))
        {
            cpu_relax();
        }
        /* Lock освободился, повторяем попытку захвата в outer loop */
    }
}

void spin_unlock(spinlock_t *l)
{
    atomic_store_explicit(&l->flag, false, memory_order_release);
}

int spin_trylock(spinlock_t *l)
{
    bool expected = false;
    return atomic_compare_exchange_strong_explicit(
               &l->flag, &expected, true,
               memory_order_acquire, /* успех - нужна синхронизация */
               memory_order_relaxed  /* неуспех - не нужна синхронизация */
               )
               ? 1
               : 0;
}

int spin_is_locked(spinlock_t *l)
{
    /* relaxed - это просто snapshot состояния без синхронизации */
    return atomic_load_explicit(&l->flag, memory_order_relaxed) ? 1 : 0;
}

unsigned long spin_lock_irqsave(spinlock_t *l)
{
    unsigned long flags = save_flags();
    local_irq_disable();
    spin_lock(l);
    return flags;
}

void spin_unlock_irqrestore(spinlock_t *l, unsigned long flags)
{
    spin_unlock(l);
    restore_flags(flags);
}

int spin_trylock_irqsave(spinlock_t *l, unsigned long *flags)
{
    unsigned long f = save_flags();
    local_irq_disable();
    if (!spin_trylock(l))
    {
        restore_flags(f);
        return 0;
    }
    if (flags)
        *flags = f;
    return 1;
}

/* Big Kernel Lock для подсистем, написанных однопроцессорными (консоль, VFS,
 * драйверы). Не рекурсивный: повторный захват на том же CPU зациклится. */
static spinlock_t g_bkl = SPINLOCK_INIT;

void bkl_lock(void)
{
    /* держатель BKL не должен быть вытеснен */
    preempt_disable();
    spin_lock(&g_bkl);
}

void bkl_unlock(void)
{
    spin_unlock(&g_bkl);
    preempt_enable();
}

unsigned long bkl_lock_irqsave(void)
{
    preempt_disable();
    return spin_lock_irqsave(&g_bkl);
}

void bkl_unlock_irqrestore(unsigned long flags)
{
    spin_unlock_irqrestore(&g_bkl, flags);
    preempt_enable();
}