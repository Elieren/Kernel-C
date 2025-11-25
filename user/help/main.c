typedef unsigned long size_t;

extern void _do_syscall_print(const char *p);
extern void _do_syscall_exit(unsigned long code);

const char cmd_htop[] = "htop     - prints information about the heap\n";
const char cmd_clear[] = "clear    - clears the terminal\n";
const char cmd_shutdown[] = "shutdown - shutdown the system\n";
const char cmd_reboot[] = "reboot   - reboots the system\n";
const char cmd_time[] = "time     - displays the current system time and uptime\n";

void _start(void)
{
    _do_syscall_print(cmd_htop);
    _do_syscall_print(cmd_clear);
    _do_syscall_print(cmd_shutdown);
    _do_syscall_print(cmd_reboot);
    _do_syscall_print(cmd_time);

    _do_syscall_exit(0);

    for (;;)
        ;
}

void _do_syscall_print(const char *p)
{
    asm volatile(
        "mov $3, %%rax\n" /* SYSCALL_PRINT_STRING */
        "mov %0, %%rdi\n"
        "mov $0x00FFFFFF, %%rsi\n"
        "int $0x80\n"
        :
        : "r"(p)
        : "rax", "rdi", "rsi");
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