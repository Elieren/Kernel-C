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
    int state;
    uint64_t *regs;
    void *kstack;
    size_t kstack_size;
    int exit_code;
    struct task *next;    /* кольцевой список задач */
    struct task *znext;   /* список зомби (отдельный указатель!) */
    void *user_mem;       // указатель на .user память
    size_t user_mem_size; // размер .user памяти
    uint32_t cwd_idx;     /* индекс директории в FS (FS_ROOT_IDX для корня) */
} task_t;

typedef struct task_info
{
    int pid;
    int state; // TASK_RUNNING, TASK_READY и т. д.
} task_info_t;

void scheduler_init(void);
/* теперь вместо pid передаём stack_size (0 = дефолт) */
void task_create(void (*entry)(void), size_t stack_size);
int task_list(task_info_t *buf, size_t max);
int task_stop(int pid);
void reap_zombies(void);
void schedule_from_isr(uint64_t *regs, uint64_t **out_regs_ptr);

task_t *get_current_task(void);
void task_exit(int exit_code);

uint64_t utask_create(void (*entry)(void),
                      size_t stack_size,
                      void *user_mem,
                      size_t user_mem_size,
                      int argc,
                      uintptr_t argv_ptr);

int task_is_alive(int pid);

int sys_chdir(const char *path);
int sys_getcwd(char *buf, size_t size);
int sys_get_cwd_idx(uint32_t *out_idx);

#endif
