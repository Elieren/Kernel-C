#ifndef SPINLOCK_H
#define SPINLOCK_H

#include <stdatomic.h>
#include <stdint.h>
#include <stdbool.h>

/* Тип спинлока — используем atomic_bool чтобы можно было читать состояние */
typedef struct
{
    atomic_bool flag;
} spinlock_t;

/* Инициализация (можно использовать SPINLOCK_INIT) */
#define SPINLOCK_INIT {ATOMIC_VAR_INIT(false)}

void fb_lock_acquire(void);
void fb_lock_release(void);

/* API с типом */
void spin_lock(spinlock_t *l);
void spin_unlock(spinlock_t *l);
int spin_trylock(spinlock_t *l);   /* возвращает 1 при захвате, 0 если уже захвачен */
int spin_is_locked(spinlock_t *l); /* возвращает 1 если захвачен, 0 если свободен */

/* Захват лока с сохранением и отключением локальных прерываний: иначе
 * прерывание на том же CPU, взявшее этот лок, даст deadlock. */
unsigned long spin_lock_irqsave(spinlock_t *l);
void spin_unlock_irqrestore(spinlock_t *l, unsigned long flags);

/* Неблокирующий вариант, 1 — захвачен */
int spin_trylock_irqsave(spinlock_t *l, unsigned long *flags);

/* Big Kernel Lock (подсистемы, написанные однопроцессорными).
 * Самый внешний лок: внутри других локов не берётся. */
void bkl_lock(void);
void bkl_unlock(void);
unsigned long bkl_lock_irqsave(void);
void bkl_unlock_irqrestore(unsigned long flags);

#endif /* SPINLOCK_H */
