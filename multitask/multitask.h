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
    int pid;
    task_state_t state;
    uint32_t *regs; /* указатель на стек-фрейм (тот, который подставляем в esp) */
    void *kstack;   /* низ стека (malloc) */
    size_t kstack_size;
    struct task *next;
} task_t;

void scheduler_init(void);
/* теперь вместо pid передаём stack_size (0 = дефолт) */
task_t *task_create(void (*entry)(void), size_t stack_size);
void schedule_from_isr(uint32_t *regs, uint32_t **out_regs_ptr);
task_t *get_current_task(void);

#endif
