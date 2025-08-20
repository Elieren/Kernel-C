#ifndef MULTITASK_H
#define MULTITASK_H

#include <stdint.h>
#include <stddef.h>

#define KSTACK_SIZE (8 * 1024) /* дефолтный размер */

typedef enum
{
    TASK_RUNNING,
    TASK_READY,
    TASK_BLOCKED,
    TASK_ZOMBIE
} task_state_t;

typedef struct task
{
    int pid;            /* уникальный идентификатор задачи (0 = init) */
    task_state_t state; /* текущее состояние */
    uint32_t *regs;     /* указатель на стек-фрейм (тот, который подставляем в ESP при переключении) */
    void *kstack;       /* указатель на начало выделенного kstack (malloc) */
    size_t kstack_size; /* размер стека в байтах */
    int exit_code;      /* код завершения (если TASK_ZOMBIE) */
    struct task *next;  /* кольцевая ссылка (tail->next = head) или список зомби */
} task_t;

typedef struct task_info
{
    int pid;
    int state; // TASK_RUNNING, TASK_READY и т. д.
} task_info_t;

void scheduler_init(void);
/* теперь вместо pid передаём stack_size (0 = дефолт) */
void task_create(void (*entry)(void), size_t stack_size);
void schedule_from_isr(uint32_t *regs, uint32_t **out_regs_ptr);
int task_list(task_info_t *buf, size_t max);
int task_stop(int pid);
void reap_zombies(void);

#endif
