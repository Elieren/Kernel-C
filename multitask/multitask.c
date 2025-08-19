// task.c
#include "multitask.h"
#include "../malloc/malloc.h"
#include "../libc/string.h"
#include "../vga/vga.h"

extern char _heap_start;
extern char _heap_end;

static task_t *task_ring = NULL; /* кольцевой список — будем хранить tail (последний элемент) */
static task_t *current = NULL;
static int next_pid = 1;

/* Статическая init-задача, чтобы в ISR не вызывать malloc */
static task_t init_task;

/* Прототипы для внешних вызовов */
void console_puts(const char *s);

/* layout стек-фрейма, который ожидает ISR:
   (stack pointer указывает на начало)
   [0]  int_no
   [1]  err_code
   [2]  EDI
   [3]  ESI
   [4]  EBP
   [5]  ESP_saved (we set current sp)
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
*/

static inline uint32_t *align_down_16(uint32_t *p)
{
    return (uint32_t *)(((uintptr_t)p) & ~0xFUL);
}

static uint32_t *prepare_initial_stack(void (*entry)(void), void *kstack_top)
{
    const int FRAME_WORDS = 17;
    uint32_t *sp = (uint32_t *)kstack_top;

    /* выравниваем стек вниз до 16 байт */
    sp = align_down_16(sp);
    sp -= FRAME_WORDS;

    /* Заполняем в порядке, который ожидает ISR (см выше).
       Используем явные значения селекторов: DS/ES/FS/GS = 0x10 (data), CS = 0x08 (code).
       При необходимости поменяйте селекторы под вашу GDT. */
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
    sp[10] = 0x10;            /* DS */
    sp[11] = 0x10;            /* ES */
    sp[12] = 0x10;            /* FS */
    sp[13] = 0x10;            /* GS */
    sp[14] = (uint32_t)entry; /* EIP -> начальная точка */
    sp[15] = 0x08;            /* CS */
    sp[16] = 0x202;           /* EFLAGS: IF = 1 */

    return sp;
}

void scheduler_init(void)
{
    /* Инициализируем статическую init-задачу (представляет контекст до первого переключения) */
    memset(&init_task, 0, sizeof(init_task));
    init_task.pid = 0;
    init_task.state = TASK_RUNNING;
    init_task.regs = NULL;
    init_task.kstack = NULL;
    init_task.next = &init_task;

    /* task_ring хранит хвост (последний вставленный). На старте — init_task */
    task_ring = &init_task;
    current = NULL;
    next_pid = 1;
}

/* Создаёт kernel-thread — выделяет стек, подготавливает стартовый frame и вставляет в кольцо */
task_t *task_create(void (*entry)(void), size_t stack_size)
{
    if (stack_size == 0)
        stack_size = KSTACK_SIZE;

    task_t *t = (task_t *)malloc(sizeof(task_t));
    if (!t)
        return NULL;

    void *kstack = malloc(stack_size);
    if (!kstack)
    {
        free(t);
        return NULL;
    }

    memset(t, 0, sizeof(*t));
    t->pid = next_pid++; /* pid теперь всегда авто */
    t->state = TASK_READY;
    t->kstack = kstack;
    t->kstack_size = stack_size;

    void *kstack_top = (char *)kstack + stack_size;
    t->regs = prepare_initial_stack(entry, kstack_top);

    /* Вставляем в кольцо */
    if (!task_ring)
    {
        task_ring = t;
        t->next = t;
    }
    else
    {
        t->next = task_ring->next;
        task_ring->next = t;
        task_ring = t; /* новый хвост */
    }

    return t;
}

/* Простая выборка следующей READY задачи (round-robin). Проход по кольцу начиная с current->next. */
static task_t *pick_next(void)
{
    if (!task_ring)
        return NULL;

    task_t *start = current ? current->next : task_ring->next; /* head = tail->next */
    task_t *it = start;

    do
    {
        if (it->state == TASK_READY || it->state == TASK_RUNNING)
            return it;
        it = it->next;
    } while (it != start);

    return NULL;
}

/* Эта функция вызывается из isr_timer_dispatch.
   regs — указатель на стек-фрейм (тот, который на текущем esp).
   out_regs_ptr — адрес куда положить regs следующей задачи (возвращаем через *out_regs_ptr),
*/
void schedule_from_isr(uint32_t *regs, uint32_t **out_regs_ptr)
{
    /* Прерывания в isr отключены — НИКАКИХ malloc/free/конкуренции здесь! */

    if (!current)
    {
        /* Первый тик — назначаем статическую init-задачу как текущую и сохраняем её regs */
        init_task.regs = regs;
        init_task.state = TASK_RUNNING;
        current = &init_task;
        /* вставка init_task уже выполнена в scheduler_init */
    }
    else
    {
        /* Сохраняем контекст текущей задачи (нормальная задача или init) */
        current->regs = regs;
        /* если текущая задача была выполняющейся, при выходе пометим её как READY */
        if (current->state == TASK_RUNNING)
            current->state = TASK_READY;
    }

    /* выбираем следующую задачу */
    task_t *next = pick_next();
    if (!next)
    {
        /* не переключаемся: оставляем тот же контекст */
        *out_regs_ptr = regs;
        return;
    }

    /* переключаемся только если next != current */
    if (next == current)
    {
        *out_regs_ptr = current->regs;
        current->state = TASK_RUNNING;
        return;
    }

    task_t *prev = current;
    current = next;
    current->state = TASK_RUNNING;
    *out_regs_ptr = current->regs;
}

/* Возвращает указатель на текущую задачу */
task_t *get_current_task(void) { return current; }
