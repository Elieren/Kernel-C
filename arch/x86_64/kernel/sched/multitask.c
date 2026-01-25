#include <asm/multitask.h>
#include <asm/tss.h>
#include <stdint.h>

uint64_t *prepare_initial_stack(void (*entry)(void),
                                void *kstack_top,
                                void *user_stack_top,
                                int argc,
                                uintptr_t argv_ptr,
                                int user_mode)
{
    const int FRAME_QWORDS = 22;
    uint64_t *sp = (uint64_t *)kstack_top;
    sp = (uint64_t *)(((uintptr_t)sp) & ~0xFULL); /* align down 16 */
    sp -= FRAME_QWORDS;

    sp[0] = 32;                  /* int_no (dummy) */
    sp[1] = 0;                   /* err_code */
    sp[2] = 0;                   /* r15 */
    sp[3] = 0;                   /* r14 */
    sp[4] = 0;                   /* r13 */
    sp[5] = 0;                   /* r12 */
    sp[6] = 0;                   /* r11 */
    sp[7] = 0;                   /* r10 */
    sp[8] = 0;                   /* r9  */
    sp[9] = 0;                   /* r8  */
    sp[10] = (uint64_t)argc;     /* rdi */
    sp[11] = (uint64_t)argv_ptr; /* rsi */
    sp[12] = 0;                  /* rbp */
    sp[13] = 0;                  /* rbx */
    sp[14] = 0;                  /* rdx */
    sp[15] = 0;                  /* rcx */
    sp[16] = 0;                  /* rax */
    sp[17] = (uint64_t)entry;    /* rip */
    sp[19] = 0x202;              /* rflags (IF=1) */

    if (user_mode)
    {
        sp[18] = USER_CS;
        sp[20] = (uint64_t)user_stack_top;
        sp[21] = USER_SS;
    }
    else
    {
        sp[18] = 0x08;
        sp[20] = (uint64_t)kstack_top;
        sp[21] = 0x10;
    }

    return sp;
}

void update_kernel_stack(uint64_t kstack_top)
{
    tss_update_rsp0(kstack_top);
}

void init_task_switching(void)
{
    tss_init();
}