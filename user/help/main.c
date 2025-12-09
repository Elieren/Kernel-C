typedef unsigned long size_t;

#define SYSCALL_PRINT_STRING 3
#define SYSCALL_TASK_EXIT 204

#define WHITE 0x00FFFFFF

void _do_syscall_print(const char *p);
void _do_syscall_exit(unsigned long code);

const char cmd_memstat[] = "memstat  - prints information about the heap\n";
const char cmd_clear[] = "clear    - clears the terminal\n";
const char cmd_shutdown[] = "shutdown - shutdown the system\n";
const char cmd_reboot[] = "reboot   - reboots the system\n";
const char cmd_time[] = "time     - displays the current system time and uptime\n";
const char cmd_ls[] = "ls       - lists files in the current directory\n";
const char cmd_pwd[] = "pwd      - prints the current working directory\n";
const char cmd_cd[] = "cd       - changes the current working directory\n";
const char cmd_mkdir[] = "mkdir    - creates a new directory\n";
const char cmd_rm[] = "rm       - removes a file or directory\n";

void _start(void)
{
    _do_syscall_print(cmd_memstat);
    _do_syscall_print(cmd_clear);
    _do_syscall_print(cmd_shutdown);
    _do_syscall_print(cmd_reboot);
    _do_syscall_print(cmd_time);
    _do_syscall_print(cmd_ls);
    _do_syscall_print(cmd_pwd);
    _do_syscall_print(cmd_cd);
    _do_syscall_print(cmd_mkdir);
    _do_syscall_print(cmd_rm);

    _do_syscall_exit(0);

    for (;;)
        asm volatile("pause");
}

void _do_syscall_print(const char *p)
{
    asm volatile(
        "int $0x80"
        :
        : "a"(SYSCALL_PRINT_STRING), "D"(p), "S"(WHITE)
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