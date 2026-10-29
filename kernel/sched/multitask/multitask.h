#ifndef MULTITASK_H
#define MULTITASK_H

#include <stdint.h>
#include <stddef.h>

#include <asm/multitask.h>
#include <asm/fpu.h>
#include "mm/uaccess/uaccess.h"

#define KSTACK_SIZE (8 * 1024) /* дефолтный размер */
#define TASK_NAME_MAX 32

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
    struct task *next;      /* кольцевой список задач */
    struct task *znext;     /* список зомби (отдельный указатель!) */
    void *user_mem;         // указатель на .user память
    size_t user_mem_size;   // размер .user памяти
    void *user_stack;       // отдельный, замапленный буфер под user-mode стек
    size_t user_stack_size; // его размер
    uint32_t cwd_idx;       /* индекс директории в FS (FS_ROOT_IDX для корня) */
    char *name;
    uint64_t *page_table;
    user_alloc_t user_allocs[USER_ALLOC_MAX];                       /* блоки из SYSCALL_MALLOC */
    uint8_t fpu_state[FPU_STATE_SIZE] __attribute__((aligned(16))); /* FXSAVE */
} task_t;

typedef struct task_info
{
    int pid;
    int state;                // TASK_RUNNING, TASK_READY и т. д.
    char name[TASK_NAME_MAX]; // '\0'-терминированная копия имени
} task_info_t;

void scheduler_init(void);
/* теперь вместо pid передаём stack_size (0 = дефолт) */
void task_create(void (*entry)(void), size_t stack_size, const char *name);
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
                      uintptr_t argv_ptr,
                      const char *name);

int task_is_alive(int pid);

int sys_chdir(const char *path);
int sys_getcwd(char *buf, size_t size);
int sys_get_cwd_idx(uint32_t *out_idx);

void task_set_foreground(int pid);
void task_kill_foreground(void);

void kill_all_tasks(void);
void emergency_terminate_all(void);

#endif