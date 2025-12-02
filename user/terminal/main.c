typedef unsigned long size_t;
typedef unsigned long uint64_t;
typedef unsigned int uint32_t;
typedef unsigned char uint8_t;

void _do_syscall_print_string(const char *p, unsigned long color);
void _do_syscall_print_char(unsigned long ch, unsigned long color);
void _do_syscall_backspace(void);
void *_do_syscall_malloc(unsigned long size);
unsigned char _do_syscall_getchar(void);
unsigned long _do_syscall_task_create(const char *cmdline);
unsigned long _do_syscall_task_is_alive(unsigned long pid);
void _do_syscall_task_stop(unsigned long pid);
void _do_syscall_throw_exception(unsigned long code, const char *msg);
void new_line(void);
int _do_syscall_chdir(const char *path);
unsigned long _do_syscall_getcwd(char *buf, unsigned long size);
int strncmp(const char *a, const char *b, size_t n);

#define SYSCALL_PRINT_CHAR 2
#define SYSCALL_PRINT_STRING 3
#define SYSCALL_BACKSPACE 4

#define SYSCALL_MALLOC 10

#define SYSCALL_GETCHAR 30
#define SYSCALL_SETPOSCURSOR 31

#define SYSCALL_TASK_CREATE 200
#define SYSCALL_TASK_IS_ALIVE 205
#define SYSCALL_TASK_STOP 202

#define THROW_AN_EXCEPTION 300

#define SYSCALL_CHDIR 500
#define SYSCALL_GETCWD 501

#define WHITE 0x00FFFFFF
#define BLACK 0x00000000

#define CTRL_C '\x03'  /* Ctrl+C (ETX) */
#define BACKSPACE '\b' /* 0x08 */
#define NEWLINE '\n'   /* 0x0A */
#define NUL '\0'       /* 0x00 */
#define TAB '\t'       /* 0x09 */
#define SPACE ' '      /* 0x20 */
#define INTERNAL_SPACE ((char)0x01)

#define DIR_BUF_SIZE 1024

const char prompt_msg[] = "$ ";
const char welcome_msg[] = "SimpleTerm v0.3";
const char error_message[] = "Command not found: ";

static uint64_t input_len = 0;
static char *input_buffer_ptr = 0;
static char *directory_buffer_ptr = 0;
static char notfound_msg[256];
static int res = 0;

static uint64_t child_pid = 0;

void _start(void)
{
    input_buffer_ptr = (char *)_do_syscall_malloc(8192);
    directory_buffer_ptr = (char *)_do_syscall_malloc(DIR_BUF_SIZE);
    input_len = 0;
    child_pid = 0;

    _do_syscall_print_string(welcome_msg, WHITE);
    new_line();

    res = _do_syscall_getcwd(directory_buffer_ptr, DIR_BUF_SIZE);
    if (res != (unsigned long)-1)
    {
        _do_syscall_print_string(directory_buffer_ptr, WHITE);
    }
    _do_syscall_print_string(prompt_msg, WHITE);

    for (;;)
    {
        unsigned char ch = _do_syscall_getchar();

        if (ch == NUL)
        {
            asm volatile("hlt");
            continue;
        }

        if (ch == CTRL_C)
        {
            if (child_pid != 0)
            {
                _do_syscall_task_stop(child_pid);
                child_pid = 0;
            }
            continue;
        }

        if (ch == NEWLINE)
        {
            if (input_buffer_ptr)
                input_buffer_ptr[input_len] = '\0';

            new_line();

            if (input_buffer_ptr && input_len > 0)
            {
                /* Проверка команды cd */
                if (strncmp(input_buffer_ptr, "cd", 2) == 0 &&
                    (input_buffer_ptr[2] == '\0' || input_buffer_ptr[2] == ' '))
                {
                    const char *arg = input_buffer_ptr + 2;
                    while (*arg == ' ')
                        arg++;

                    if (*arg == '\0')
                        arg = "/";

                    int ch_res = _do_syscall_chdir(arg);
                    if (ch_res != 0)
                    {
                        size_t i = 0;
                        const char *s = "cd: failed: ";
                        while (*s && i + 2 < sizeof notfound_msg)
                            notfound_msg[i++] = *s++;
                        const char *p = arg;
                        while (*p && i + 2 < sizeof notfound_msg)
                            notfound_msg[i++] = *p++;
                        notfound_msg[i++] = '\n';
                        notfound_msg[i] = '\0';

                        _do_syscall_throw_exception(3, notfound_msg);
                    }

                    res = _do_syscall_getcwd(directory_buffer_ptr, DIR_BUF_SIZE);
                    if (res != (unsigned long)-1)
                    {
                        _do_syscall_print_string(directory_buffer_ptr, WHITE);
                    }
                    _do_syscall_print_string(prompt_msg, WHITE);
                }
                else
                {
                    /* Запуск внешней команды */
                    unsigned long pid = _do_syscall_task_create(input_buffer_ptr);
                    if (pid == 0)
                    {
                        size_t i = 0;
                        const char *s = error_message;
                        while (*s && i + 2 < sizeof notfound_msg)
                            notfound_msg[i++] = *s++;
                        const char *p = input_buffer_ptr;
                        while (*p && i + 2 < sizeof notfound_msg)
                            notfound_msg[i++] = *p++;
                        notfound_msg[i++] = '\n';
                        notfound_msg[i] = '\0';
                        _do_syscall_throw_exception(3, notfound_msg);

                        res = _do_syscall_getcwd(directory_buffer_ptr, DIR_BUF_SIZE);
                        if (res != (unsigned long)-1)
                        {
                            _do_syscall_print_string(directory_buffer_ptr, WHITE);
                        }
                        _do_syscall_print_string(prompt_msg, WHITE);
                    }
                    else
                    {
                        child_pid = pid;
                        while (_do_syscall_task_is_alive(child_pid) != 0)
                        {
                            asm volatile("hlt");
                        }
                        child_pid = 0;

                        /* после завершения команды обновим cwd и покажем приглашение */
                        res = _do_syscall_getcwd(directory_buffer_ptr, DIR_BUF_SIZE);
                        if (res != (unsigned long)-1)
                        {
                            _do_syscall_print_string(directory_buffer_ptr, WHITE);
                        }
                        _do_syscall_print_string(prompt_msg, WHITE);
                    }
                }
            }
            else
            {
                /* пустая команда - просто показать приглашение снова */
                res = _do_syscall_getcwd(directory_buffer_ptr, DIR_BUF_SIZE);
                if (res != (unsigned long)-1)
                {
                    _do_syscall_print_string(directory_buffer_ptr, WHITE);
                }
                _do_syscall_print_string(prompt_msg, WHITE);
            }

            input_len = 0;
            asm volatile("hlt");
            continue;
        }

        /* Backspace */
        if (ch == BACKSPACE)
        {
            if (input_len == 0)
            {
                asm volatile("hlt");
                continue;
            }
            input_len -= 1;
            _do_syscall_backspace();
            asm volatile("hlt");
            continue;
        }

        if (ch == (unsigned char)INTERNAL_SPACE)
        {
            ch = SPACE;
        }

        if (input_buffer_ptr && input_len + 1 < 8192)
        {
            input_buffer_ptr[input_len++] = (char)ch;
        }

        _do_syscall_print_char((unsigned long)ch, WHITE);

        asm volatile("hlt");
    }

    for (;;)
        asm volatile("hlt");
}

int strncmp(const char *a, const char *b, size_t n)
{
    if (n == 0)
        return 0;
    unsigned char ca, cb;
    while (n--)
    {
        ca = (unsigned char)*a++;
        cb = (unsigned char)*b++;
        if (ca != cb)
            return (ca < cb) ? -1 : 1;
        if (ca == 0)
            return 0;
    }
    return 0;
}

void _do_syscall_print_string(const char *p, unsigned long color)
{
    asm volatile(
        "int $0x80"
        :
        : "a"(SYSCALL_PRINT_STRING), "D"(p), "S"(color)
        : "rcx", "r11", "memory");
}

void _do_syscall_print_char(unsigned long ch, unsigned long color)
{
    asm volatile(
        "int $0x80"
        :
        : "a"(SYSCALL_PRINT_CHAR), "D"(ch), "S"(color)
        : "rcx", "r11", "memory");
}

void _do_syscall_backspace(void)
{
    asm volatile(
        "int $0x80"
        :
        : "a"(SYSCALL_BACKSPACE)
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

unsigned char _do_syscall_getchar(void)
{
    unsigned long ret;
    asm volatile(
        "int $0x80"
        : "=a"(ret)
        : "a"(SYSCALL_GETCHAR)
        : "rcx", "r11", "memory");
    return (unsigned char)(ret & 0xFFUL);
}

unsigned long _do_syscall_task_create(const char *cmdline)
{
    unsigned long ret;
    asm volatile(
        "int $0x80"
        : "=a"(ret)
        : "a"(SYSCALL_TASK_CREATE), "D"(cmdline)
        : "rcx", "r11", "memory");
    return ret;
}

unsigned long _do_syscall_task_is_alive(unsigned long pid)
{
    unsigned long ret;
    asm volatile(
        "int $0x80"
        : "=a"(ret)
        : "a"(SYSCALL_TASK_IS_ALIVE), "D"(pid)
        : "rcx", "r11", "memory");
    return ret;
}

void _do_syscall_task_stop(unsigned long pid)
{
    asm volatile(
        "int $0x80"
        :
        : "a"(SYSCALL_TASK_STOP), "D"(pid)
        : "rcx", "r11", "memory");
}

void _do_syscall_throw_exception(unsigned long code, const char *msg)
{
    asm volatile(
        "int $0x80"
        :
        : "a"(THROW_AN_EXCEPTION), "D"(code), "S"(msg)
        : "rcx", "r11", "memory");
}

void new_line(void)
{
    _do_syscall_print_char((unsigned long)10, WHITE);
}

int _do_syscall_chdir(const char *path)
{
    long ret;
    asm volatile(
        "int $0x80"
        : "=a"(ret)
        : "a"(SYSCALL_CHDIR), "D"(path)
        : "rcx", "r11", "memory");
    return (int)ret;
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