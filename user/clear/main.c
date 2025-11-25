typedef unsigned long size_t;

extern void _do_syscall_clean_screen(void);
extern void _do_syscall_exit(unsigned long code);

void _start(void)
{
    _do_syscall_clean_screen();
    _do_syscall_exit(0);

    for (;;)
        ;
}

void _do_syscall_clean_screen(void)
{
    asm volatile(
        "mov $6, %%rax\n" /* SYSCALL_CLEAN_SCREEN */
        "int $0x80\n"
        :
        :
        : "rax");
}

void _do_syscall_exit(unsigned long code)
{
    asm volatile(
        "mov $204, %%rax\n" /* SYSCALL_TASK_EXIT */
        "mov %0, %%rdi\n"
        "int $0x80\n"
        :
        : "r"(code)
        : "rax", "rdi");
}