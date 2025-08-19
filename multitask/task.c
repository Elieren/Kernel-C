// task.c
#include "task.h"
#include "../malloc/malloc.h"
#include "../libc/string.h"
#include "../vga/vga.h"

extern char _heap_start;
extern char _heap_end;

static task_t *task_ring = NULL; /* кольцевой список задач */
static task_t *current = NULL;
static int next_pid = 1;

/* Прототипы для внешних вызовов */
void console_puts(const char *s);

/* layout стек-фрейма, который ожидает ISR:
   (stack pointer указывает на начало)
   [0]  int_no
   [1]  err_code
   [2]  EDI
   [3]  ESI
   [4]  EBP
   [5]  ESP_saved (we set 0)
   [6]  EBX
   [7]  EDX
   [8]  ECX
   [9]  EAX
   [10] DS
   [11] ES
   [12] FS
   [13] GS
   [14] EIP
   [15] CS
   [16] EFLAGS
   После mov esp = regs; ISR делает: pop int_no; pop err_code; popa; pop ds; pop es; pop fs; pop gs; iretd
*/

static uint32_t *prepare_initial_stack(void (*entry)(void), void *kstack_top)
{
    /* создаём фрейм из 17 uint32_t слов */
    const int FRAME_WORDS = 17;
    uint32_t *sp = (uint32_t *)kstack_top;

    /* выравниваем */
    sp = (uint32_t *)((uintptr_t)sp & ~0xFUL);
    sp -= FRAME_WORDS;

    /* Заполняем в порядке, который ожидает ISR (см выше) */
    sp[0] = 32;               /* int_no (dummy) */
    sp[1] = 0;                /* err_code */
    sp[2] = 0;                /* EDI */
    sp[3] = 0;                /* ESI */
    sp[4] = 0;                /* EBP */
    sp[5] = (uint32_t)sp;     /* ESP_saved (можно записать текущий sp) */
    sp[6] = 0;                /* EBX */
    sp[7] = 0;                /* EDX */
    sp[8] = 0;                /* ECX */
    sp[9] = 0;                /* EAX */
    sp[10] = 0;               /* DS */
    sp[11] = 0;               /* ES */
    sp[12] = 0;               /* FS */
    sp[13] = 0;               /* GS */
    sp[14] = (uint32_t)entry; /* EIP -> начальная точка */
    sp[15] = 0x08;            /* CS (кодовый селектор — как у вас в idt_set_gate) */
    sp[16] = 0x202;           /* EFLAGS: IF = 1 */

    return sp;
}

void scheduler_init(void)
{
    task_ring = NULL;
    current = NULL;
    next_pid = 1;
}

/* Создаёт kernel-thread — выделяет стек, подготавливает стартовый frame и вставляет в круг */
task_t *task_create(void (*entry)(void), int pid)
{
    task_t *t = (task_t *)malloc(sizeof(task_t));
    if (!t)
        return NULL;

    void *kstack = malloc(KSTACK_SIZE);
    if (!kstack)
    {
        free(t);
        return NULL;
    }

    memset(t, 0, sizeof(*t));
    t->pid = (pid > 0) ? pid : next_pid++;
    t->state = TASK_READY;
    t->kstack = kstack;

    void *kstack_top = (char *)kstack + KSTACK_SIZE;
    t->regs = prepare_initial_stack(entry, kstack_top);

    if (!task_ring)
    {
        task_ring = t;
        t->next = t;
    }
    else
    {
        /* вставляем сразу после головы */
        t->next = task_ring->next;
        task_ring->next = t;
    }

    return t;
}

/* Простая выборка следующей READY задачи (round-robin). */
static task_t *pick_next(void)
{
    if (!task_ring)
        return NULL;
    task_t *it = (current ? current->next : task_ring);
    /* ищем следующую READY; если нет — вернём текущую */
    for (int i = 0; i < 4096; ++i)
    {
        if (it->state == TASK_READY || it->state == TASK_RUNNING)
            return it;
        it = it->next;
    }
    return NULL;
}

/* Эта функция вызывается из isr_timer_dispatch.
   regs — указатель на стек-фрейм (тот, который на текущем esp).
   out_regs_ptr — адрес куда положить regs следующей задачи (возвращаем через *out_regs_ptr),
   также функция возвращает void.
*/
void schedule_from_isr(uint32_t *regs, uint32_t **out_regs_ptr)
{
    /* regs указывает на фрейм current контекста (мы в прерывании) */
    if (!current)
    {
        /* Первый тик — создадим "init" задачу, которая представляет текущее выполнение ядра */
        current = (task_t *)malloc(sizeof(task_t));
        memset(current, 0, sizeof(*current));
        current->pid = 0;
        current->state = TASK_RUNNING;
        current->regs = regs;
        current->kstack = NULL;

        if (!task_ring)
        {
            task_ring = current;
            current->next = current;
        }
        else
        {
            current->next = task_ring->next;
            task_ring->next = current;
        }
    }
    else
    {
        /* Сохраняем контекст текущей задачи */
        current->regs = regs;
        current->state = TASK_READY;
    }

    /* выбираем следующую задачу */
    task_t *next = pick_next();
    if (!next)
    {
        /* не переключаемся */
        *out_regs_ptr = regs;
        return;
    }

    /* переключаем */
    task_t *prev = current;
    current = next;
    current->state = TASK_RUNNING;
    *out_regs_ptr = current->regs;
}

/* Возвращает указатель на текущую задачу */
task_t *get_current_task(void) { return current; }
