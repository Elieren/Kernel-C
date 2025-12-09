typedef unsigned long size_t;

#define SYSCALL_PRINT_STRING 3
#define SYSCALL_GETCWD 501

#define SYSCALL_MALLOC 10
#define SYSCALL_FREE 12

#define SYSCALL_TASK_EXIT 204

#define WHITE 0x00FFFFFF

#define DIR_BUF_SIZE 1024

size_t strlen(const char *s);
void _do_syscall_print_string(const char *p, unsigned long color);
void *_do_syscall_malloc(unsigned long size);
void _do_syscall_free(void *ptr);
unsigned long _do_syscall_getcwd(char *buf, unsigned long size);
void _do_syscall_exit(unsigned long code);

static int res = 0;
static char *directory_buffer_ptr = (char *)0;

void _start(void)
{
    directory_buffer_ptr = (char *)_do_syscall_malloc(DIR_BUF_SIZE);

    res = _do_syscall_getcwd(directory_buffer_ptr, DIR_BUF_SIZE);

    if (res != -1)
    {
        size_t len = strlen(directory_buffer_ptr);

        /* Проверка: нужно место для '\n' и нового '\0' */
        if (len + 2 <= DIR_BUF_SIZE)
        {
            directory_buffer_ptr[len] = '\n';
            directory_buffer_ptr[len + 1] = '\0';
        }

        _do_syscall_print_string(directory_buffer_ptr, WHITE);
    }

    _do_syscall_free(directory_buffer_ptr);

    _do_syscall_exit(0);

    for (;;)
        asm volatile("pause");
}

size_t strlen(const char *s)
{
    const char *p = s;
    while (*p)
        ++p;
    return (size_t)(p - s);
}

void _do_syscall_print_string(const char *p, unsigned long color)
{
    asm volatile(
        "int $0x80"
        :
        : "a"(SYSCALL_PRINT_STRING), "D"(p), "S"(color)
        : "rcx", "r11", "memory");
}

void *_do_syscall_malloc(unsigned long size)
{
    void *ret;
    asm volatile(
        "int $0x80"
        : "=a"(ret)
        : "a"(SYSCALL_MALLOC), "D"(size)
        : "rcx", "r11", "memory");
    return ret;
}

void _do_syscall_free(void *ptr)
{
    unsigned long ret;
    asm volatile(
        "int $0x80"
        : "=a"(ret)
        : "a"(SYSCALL_FREE), "D"(ptr)
        : "rcx", "r11", "memory");
    (void)ret;
}

unsigned long _do_syscall_getcwd(char *buf, unsigned long size)
{
    unsigned long ret;
    asm volatile("int $0x80"
                 : "=a"(ret)
                 : "a"(SYSCALL_GETCWD), "D"(buf), "S"(size)
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