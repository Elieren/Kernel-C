typedef unsigned long size_t;

#define SYSCALL_CLEAN_SCREEN 6
#define SYSCALL_TASK_EXIT 204

void _do_syscall_clean_screen(void);
void _do_syscall_exit(unsigned long code);

void _start(void)
{
    _do_syscall_clean_screen();
    _do_syscall_exit(0);

    for (;;)
        asm volatile("pause");
}

void _do_syscall_clean_screen(void)
{
    asm volatile(
        "int $0x80"
        :
        : "a"(SYSCALL_CLEAN_SCREEN)
        : "rcx", "r11", "memory");
}

void _do_syscall_exit(unsigned long code)
{
    asm volatile(
        "int $0x80"
        :
        : "a"(SYSCALL_TASK_EXIT), "D"(code)
        : "rcx", "r11");
}