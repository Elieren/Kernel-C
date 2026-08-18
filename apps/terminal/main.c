typedef unsigned long size_t;
typedef unsigned long uint64_t;
typedef unsigned int uint32_t;
typedef unsigned char uint8_t;

/* ---- Объявления --------------------------------------------------- */
void _do_syscall_print_string(const char *p, unsigned long color);
void _do_syscall_print_char(unsigned long ch, unsigned long color);
void _do_syscall_backspace(void);
void *_do_syscall_malloc(unsigned long size);
unsigned char _do_syscall_getchar(void);
unsigned long _do_syscall_task_create(const char *cmdline);
unsigned long _do_syscall_task_is_alive(unsigned long pid);
void _do_syscall_throw_exception(unsigned long code, const char *msg);
void _do_syscall_set_foreground(unsigned long pid);
int _do_syscall_chdir(const char *path);
unsigned long _do_syscall_getcwd(char *buf, unsigned long size);
void new_line(void);
int strncmp(const char *a, const char *b, size_t n);

/* строковые хелперы */
size_t s_strlen(const char *s);
void s_strlcpy(char *dst, const char *src, size_t max);
void s_memcpy(void *dst, const void *src, size_t n);
void s_memset(void *p, unsigned char v, size_t n);

void show_prompt(void);

/* ---- Константы ---------------------------------------------------- */
#define SYSCALL_PRINT_CHAR 2
#define SYSCALL_PRINT_STRING 3
#define SYSCALL_BACKSPACE 4
#define SYSCALL_MALLOC 10
#define SYSCALL_GETCHAR 30
#define SYSCALL_TASK_CREATE 200
#define SYSCALL_TASK_IS_ALIVE 205
#define SYSCALL_SET_FOREGROUND 206
#define THROW_AN_EXCEPTION 300
#define SYSCALL_CHDIR 500
#define SYSCALL_GETCWD 501

#define WHITE 0x00FFFFFF
#define BLACK 0x00000000

#define BACKSPACE '\b' /* 0x08 */
#define NEWLINE '\n'   /* 0x0A */
#define ESC '\x1B'     /* 0x1B */
#define NUL '\0'       /* 0x00 */
#define SPACE ' '      /* 0x20 */
#define INTERNAL_ESC '\x1B'

#define DIR_BUF_SIZE 1024
#define INPUT_BUF_SIZE 8192
#define ERRMSG_BUF_SIZE 256

/* ---- Глобальные переменные ---------------------------------------- */
const char prompt_msg[] = "$ ";
const char welcome_msg[] = "SimpleTerm v0.4";
const char err_cmd_msg[] = "Command not found: ";
const char err_cd_msg[] = "cd: failed: ";

uint64_t input_len = 0;
char *input_buf = 0;
char *dir_buf = 0;
char errmsg[ERRMSG_BUF_SIZE];
unsigned long cwd_res = 0;
uint64_t child_pid = 0;

/* ESC-автомат: 0=нормальный, 1=получили ESC, 2=получили ESC[ */
int esc_state = 0;

/* ============================================================
 * Точка входа
 * ============================================================ */
void _start(void)
{
    input_buf = (char *)_do_syscall_malloc(INPUT_BUF_SIZE);
    dir_buf = (char *)_do_syscall_malloc(DIR_BUF_SIZE);

    if (!input_buf || !dir_buf)
        for (;;)
            asm volatile("pause");

    input_len = 0;
    child_pid = 0;
    esc_state = 0;

    _do_syscall_set_foreground(0);

    _do_syscall_print_string(welcome_msg, WHITE);
    new_line();
    show_prompt();

    for (;;)
    {
        unsigned char ch = _do_syscall_getchar();

        if (ch == (unsigned char)NUL)
        {
            asm volatile("pause");
            continue;
        }

        /* ---- ESC-автомат (ANSI VT100: ESC [ A/B/C/D) ------------ */
        if (esc_state == 0 && ch == (unsigned char)ESC)
        {
            esc_state = 1;
            asm volatile("pause");
            continue;
        }

        if (esc_state == 1)
        {
            esc_state = (ch == '[') ? 2 : 0;
            asm volatile("pause");
            continue;
        }

        if (esc_state == 2)
        {
            esc_state = 0;
            /* ESC [ A/B/C/D (стрелки) — история отключена, игнорируем */
            asm volatile("pause");
            continue;
        }

        /* ---- Enter ----------------------------------------------- */
        if (ch == (unsigned char)NEWLINE)
        {
            esc_state = 0;
            if (input_buf)
                input_buf[input_len] = '\0';
            new_line();

            if (input_buf && input_len > 0)
            {
                /* -- команда cd -- */
                if (strncmp(input_buf, "cd", 2) == 0 &&
                    (input_buf[2] == '\0' || input_buf[2] == ' '))
                {
                    const char *arg = input_buf + 2;
                    while (*arg == ' ')
                        arg++;
                    if (*arg == '\0')
                        arg = "/";

                    int cd_res = _do_syscall_chdir(arg);
                    if (cd_res != 0)
                    {
                        size_t i = 0;
                        const char *s = err_cd_msg;
                        while (*s && i + 2 < ERRMSG_BUF_SIZE)
                            errmsg[i++] = *s++;
                        const char *p = arg;
                        while (*p && i + 2 < ERRMSG_BUF_SIZE)
                            errmsg[i++] = *p++;
                        errmsg[i++] = '\n';
                        errmsg[i] = '\0';
                        _do_syscall_throw_exception(3, errmsg);
                    }
                }
                else
                {
                    /* -- внешняя программа -- */
                    unsigned long pid = _do_syscall_task_create(input_buf);
                    if (pid == 0)
                    {
                        size_t i = 0;
                        const char *s = err_cmd_msg;
                        while (*s && i + 2 < ERRMSG_BUF_SIZE)
                            errmsg[i++] = *s++;
                        const char *p = input_buf;
                        while (*p && i + 2 < ERRMSG_BUF_SIZE)
                            errmsg[i++] = *p++;
                        errmsg[i++] = '\n';
                        errmsg[i] = '\0';
                        _do_syscall_throw_exception(3, errmsg);
                    }
                    else
                    {
                        child_pid = pid;

                        /*
                         * Сообщаем ядру кто foreground.
                         * Ctrl+C обработает ядро напрямую из IRQ —
                         * терминал просто ждёт task_is_alive()==0.
                         */
                        _do_syscall_set_foreground(child_pid);

                        while (_do_syscall_task_is_alive(child_pid) != 0)
                            asm volatile("pause");

                        _do_syscall_set_foreground(0);
                        child_pid = 0;
                    }
                }
            }

            input_len = 0;
            show_prompt();
            asm volatile("pause");
            continue;
        }

        /* ---- Backspace ------------------------------------------- */
        if (ch == (unsigned char)BACKSPACE)
        {
            esc_state = 0;
            if (input_len > 0)
            {
                input_len--;
                _do_syscall_backspace();
            }
            asm volatile("pause");
            continue;
        }

        /* ---- Обычный символ -------------------------------------- */
        esc_state = 0;

        if (ch == (unsigned char)INTERNAL_ESC)
            continue;

        if (input_buf && input_len + 1 < INPUT_BUF_SIZE)
        {
            input_buf[input_len++] = (char)ch;
            _do_syscall_print_char((unsigned long)ch, WHITE);
        }

        asm volatile("pause");
    }

    for (;;)
        asm volatile("pause");
}

/* ============================================================
 * strncmp
 * ============================================================ */
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

/* ============================================================
 * Строковые хелперы
 * ============================================================ */
size_t s_strlen(const char *s)
{
    size_t n = 0;
    while (s[n])
        n++;
    return n;
}

void s_strlcpy(char *dst, const char *src, size_t max)
{
    size_t i = 0;
    if (!max)
        return;
    while (i < max && src[i])
    {
        dst[i] = src[i];
        i++;
    }
    dst[i] = '\0';
}

void s_memcpy(void *dst, const void *src, size_t n)
{
    unsigned char *d = (unsigned char *)dst;
    const unsigned char *s = (const unsigned char *)src;
    for (size_t i = 0; i < n; i++)
        d[i] = s[i];
}

void s_memset(void *p, unsigned char v, size_t n)
{
    unsigned char *d = (unsigned char *)p;
    for (size_t i = 0; i < n; i++)
        d[i] = v;
}

/* ============================================================
 * Показать приглашение
 * ============================================================ */
void show_prompt(void)
{
    cwd_res = _do_syscall_getcwd(dir_buf, DIR_BUF_SIZE);
    if (cwd_res != (unsigned long)-1)
        _do_syscall_print_string(dir_buf, WHITE);
    _do_syscall_print_string(prompt_msg, WHITE);
}

/* ============================================================
 * Syscall-обёртки
 * ============================================================ */
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

void _do_syscall_throw_exception(unsigned long code, const char *msg)
{
    asm volatile(
        "int $0x80"
        :
        : "a"(THROW_AN_EXCEPTION), "D"(code), "S"(msg)
        : "rcx", "r11", "memory");
}

void _do_syscall_set_foreground(unsigned long pid)
{
    asm volatile(
        "int $0x80"
        :
        : "a"(SYSCALL_SET_FOREGROUND), "D"(pid)
        : "rcx", "r11", "memory");
}

void new_line(void)
{
    _do_syscall_print_char((unsigned long)'\n', WHITE);
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
    asm volatile(
        "int $0x80"
        : "=a"(ret)
        : "a"(SYSCALL_GETCWD), "D"(buf), "S"(size)
        : "rcx", "r11", "memory");
    return ret;
}