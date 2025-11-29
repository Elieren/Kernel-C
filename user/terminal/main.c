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
void _do_syscall_chdir(const char *path);
unsigned long _do_syscall_getcwd(char *buf, unsigned long size);

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

#define INTERNAL_SPACE 0x01

#define DIR_BUF_SIZE 1024

const char prompt_msg[] = "$ ";
const char welcome_msg[] = "SimpleTerm v0.2";
const char error_message[] = "Command not found: ";

static uint64_t input_len = 0;
static char *input_buffer_ptr = (char *)0;
static char *directory_buffer_ptr = (char *)0;
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

    if (res != -1)
    {
        _do_syscall_print_string(directory_buffer_ptr, WHITE);
    }

    _do_syscall_print_string(prompt_msg, WHITE);

    for (;;)
    {
        unsigned char ch = _do_syscall_getchar();

        if (ch == 0 || ch == 32)
        {
            asm volatile("hlt");
            continue;
        }

        if (ch == 0x03)
        {
            if (child_pid != 0)
            {
                _do_syscall_task_stop(child_pid);
                child_pid = 0;
            }
            continue;
        }

        if (ch == 10)
        {
            if (input_buffer_ptr)
            {
                input_buffer_ptr[input_len] = '\0';
            }

            new_line();

            if (input_buffer_ptr)
            {
                unsigned long pid = _do_syscall_task_create(input_buffer_ptr);
                if (pid == 0)
                {
                    size_t i = 0;
                    const char *s = error_message;

                    while (*s && i + 2 < sizeof notfound_msg)
                    {
                        notfound_msg[i++] = *s++;
                    }

                    if (input_buffer_ptr)
                    {
                        const char *p = input_buffer_ptr;
                        while (*p && i + 2 < sizeof notfound_msg)
                        {
                            notfound_msg[i++] = *p++;
                        }
                    }

                    notfound_msg[i++] = '\n';
                    notfound_msg[i] = '\0';

                    _do_syscall_throw_exception(3, notfound_msg);
                }
                else
                {
                    child_pid = pid;

                    while (_do_syscall_task_is_alive(child_pid) != 0)
                    {
                        asm volatile("hlt");
                    }
                    child_pid = 0;
                }
            }

            input_len = 0;

            res = _do_syscall_getcwd(directory_buffer_ptr, DIR_BUF_SIZE);

            if (res != -1)
            {
                _do_syscall_print_string(directory_buffer_ptr, WHITE);
            }

            _do_syscall_print_string(prompt_msg, WHITE);

            asm volatile("hlt");
            continue;
        }

        if (ch == 8)
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
            ch = 32;
        }

        if (input_buffer_ptr)
        {
            input_buffer_ptr[input_len] = (char)ch;
            input_len += 1;
        }

        _do_syscall_print_char((unsigned long)ch, WHITE);

        asm volatile("hlt");
    }

    for (;;)
        asm volatile("hlt");
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

void _do_syscall_chdir(const char *path)
{
    asm volatile("int $0x80"
                 :
                 : "a"(SYSCALL_CHDIR), "D"(path)
                 : "rcx", "r11", "memory");
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