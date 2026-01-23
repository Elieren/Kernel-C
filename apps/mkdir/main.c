typedef unsigned long size_t;
typedef unsigned long uint64_t;
typedef unsigned int uint32_t;
typedef unsigned char uint8_t;
typedef unsigned long uintptr_t;

#define SYSCALL_PRINT_STRING 3
#define SYSCALL_TASK_EXIT 204
#define SYSCALL_GET_CWD_IDX 502
#define SYSCALL_FS_MKDIR 600

#define WHITE 0x00FFFFFF
#define RED 0x00FF0000

void _do_syscall_print_string(const char *p, unsigned long color);
long _do_syscall_get_cwd_idx(uint32_t *out_idx);
int _do_syscall_fs_mkdir(const char *name, int parent);
void _do_syscall_exit(unsigned long code);

void _start(int argc, char **argv)
{
    // Проверяем количество аргументов
    if (argc < 2)
    {
        _do_syscall_print_string("Usage: mkdir <directory_name>\n", RED);
        _do_syscall_exit(1);
        for (;;)
            asm volatile("pause");
    }

    // Получаем индекс текущей директории
    uint32_t cwd_idx = 0;
    long status = _do_syscall_get_cwd_idx(&cwd_idx);
    if (status != 0)
    {
        _do_syscall_print_string("Error: cannot get current directory\n", RED);
        _do_syscall_exit(1);
        for (;;)
            asm volatile("pause");
    }

    // Создаём директорию
    const char *dirname = argv[1];
    int result = _do_syscall_fs_mkdir(dirname, (int)cwd_idx);

    if (result < 0)
    {
        _do_syscall_print_string("Error: cannot create directory '", RED);
        _do_syscall_print_string(dirname, RED);
        _do_syscall_print_string("'\n", RED);
        _do_syscall_exit(1);
        for (;;)
            asm volatile("pause");
    }

    _do_syscall_exit(0);
    for (;;)
        asm volatile("pause");
}

void _do_syscall_print_string(const char *p, unsigned long color)
{
    asm volatile(
        "int $0x80"
        :
        : "a"(SYSCALL_PRINT_STRING), "D"(p), "S"(color)
        : "rcx", "r11", "memory");
}

long _do_syscall_get_cwd_idx(uint32_t *out_idx)
{
    long ret;
    asm volatile(
        "int $0x80"
        : "=a"(ret)
        : "a"(SYSCALL_GET_CWD_IDX), "D"(out_idx)
        : "rcx", "r11", "memory");
    return ret;
}

int _do_syscall_fs_mkdir(const char *name, int parent)
{
    int ret;
    asm volatile(
        "int $0x80"
        : "=a"(ret)
        : "a"(SYSCALL_FS_MKDIR), "D"(name), "S"(parent)
        : "rcx", "r11", "memory");
    return ret;
}

void _do_syscall_exit(unsigned long code)
{
    asm volatile(
        "int $0x80"
        :
        : "a"(SYSCALL_TASK_EXIT), "D"(code)
        : "rcx", "r11");
}