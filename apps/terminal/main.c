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
int _do_syscall_fs_write_file(const char *name, const char *ext, int parent,
                              const void *data, unsigned long size);
int _do_syscall_fs_read_file(const char *name, const char *ext, int parent,
                             void *buf, unsigned long bufsize,
                             unsigned long *out_size);
int _do_syscall_chdir(const char *path);
unsigned long _do_syscall_getcwd(char *buf, unsigned long size);
void new_line(void);
int strncmp(const char *a, const char *b, size_t n);

/* строковые хелперы */
size_t s_strlen(const char *s);
void s_strlcpy(char *dst, const char *src, size_t max);
void s_memcpy(void *dst, const void *src, size_t n);
void s_memset(void *p, unsigned char v, size_t n);

/* история */
void history_load(void);
void history_save(void);
void history_add(const char *cmd);
void history_nav_up(void);
void history_nav_down(void);
void clear_input_line(void);
void set_input_line(const char *text, size_t len);
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
#define SYSCALL_FS_WRITE_FILE_IN_DIR 608
#define SYSCALL_FS_READ_FILE_IN_DIR 609

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
#define HISTORY_MAX 50
#define HISTORY_ENTRY_MAX 128
#define HISTORY_FILE_BUF 8192
#define FS_ROOT_IDX 0

/* ---- Глобальные переменные ---------------------------------------- */
const char prompt_msg[] = "$ ";
const char welcome_msg[] = "SimpleTerm v0.5";
const char err_cmd_msg[] = "Command not found: ";
const char err_cd_msg[] = "cd: failed: ";
const char hist_fname[] = "history";
const char hist_fext[] = "";

uint64_t input_len = 0;
char *input_buf = 0;
char *dir_buf = 0;
char errmsg[ERRMSG_BUF_SIZE];
unsigned long cwd_res = 0;
uint64_t child_pid = 0;

/* ESC-автомат: 0=нормальный, 1=получили ESC, 2=получили ESC[ */
int esc_state = 0;

/* История */
char *hist_pool = 0; /* HISTORY_MAX * HISTORY_ENTRY_MAX байт  */
char *hist_fbuf = 0; /* буфер файлового I/O                   */
char *hist_save = 0; /* сохранение текущего ввода при навигации*/
int hist_count = 0;
int hist_idx = -1; /* -1 = не в режиме навигации         */
int hist_save_len = 0;

/* ============================================================
 * Точка входа
 * ============================================================ */
void _start(void)
{
    input_buf = (char *)_do_syscall_malloc(INPUT_BUF_SIZE);
    dir_buf = (char *)_do_syscall_malloc(DIR_BUF_SIZE);
    hist_pool = (char *)_do_syscall_malloc(HISTORY_MAX * HISTORY_ENTRY_MAX);
    hist_fbuf = (char *)_do_syscall_malloc(HISTORY_FILE_BUF);
    hist_save = (char *)_do_syscall_malloc(HISTORY_ENTRY_MAX);

    if (!input_buf || !dir_buf || !hist_pool || !hist_fbuf || !hist_save)
        for (;;)
            asm volatile("pause");

    s_memset(hist_pool, 0, HISTORY_MAX * HISTORY_ENTRY_MAX);
    s_memset(hist_fbuf, 0, HISTORY_FILE_BUF);
    s_memset(hist_save, 0, HISTORY_ENTRY_MAX);

    input_len = 0;
    hist_count = 0;
    hist_idx = -1;
    hist_save_len = 0;
    child_pid = 0;
    esc_state = 0;

    _do_syscall_set_foreground(0);
    history_load();

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
            if (ch == 'A')
                history_nav_up(); /* ESC [ A — ↑ */
            else if (ch == 'B')
                history_nav_down(); /* ESC [ B — ↓ */
            /* ESC [ C/D (→←) — игнорируем */
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

            hist_idx = -1;

            if (input_buf && input_len > 0)
            {
                /* -- команда cd -- */
                if (strncmp(input_buf, "cd", 2) == 0 &&
                    (input_buf[2] == '\0' || input_buf[2] == ' '))
                {
                    history_add(input_buf);

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
                        history_add(input_buf);
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
 * История команд
 * ============================================================ */

/* Указатель на i-ю запись в пуле */
static char *hist_entry(int i)
{
    return hist_pool + (size_t)i * HISTORY_ENTRY_MAX;
}

void history_load(void)
{
    if (!hist_pool || !hist_fbuf)
        return;
    hist_count = 0;

    unsigned long read_bytes = 0;
    int rc = _do_syscall_fs_read_file(hist_fname, hist_fext, FS_ROOT_IDX,
                                      hist_fbuf, HISTORY_FILE_BUF - 1,
                                      &read_bytes);
    if (rc != 0 || read_bytes == 0)
        return;

    hist_fbuf[read_bytes] = '\0';

    const char *p = hist_fbuf;
    while (*p && hist_count < HISTORY_MAX)
    {
        const char *end = p;
        while (*end && *end != '\n')
            end++;
        size_t len = (size_t)(end - p);
        if (len > 0 && len < HISTORY_ENTRY_MAX)
        {
            char *dst = hist_entry(hist_count);
            s_memcpy(dst, p, len);
            dst[len] = '\0';
            hist_count++;
        }
        p = (*end == '\n') ? end + 1 : end;
    }
}

void history_save(void)
{
    if (!hist_pool || !hist_fbuf || hist_count == 0)
        return;

    size_t pos = 0;
    for (int i = 0; i < hist_count; i++)
    {
        const char *e = hist_entry(i);
        size_t len = s_strlen(e);
        if (!len)
            continue;
        if (pos + len + 1 >= HISTORY_FILE_BUF)
            break;
        s_memcpy(hist_fbuf + pos, e, len);
        pos += len;
        hist_fbuf[pos++] = '\n';
    }

    if (pos > 0)
        _do_syscall_fs_write_file(hist_fname, hist_fext, FS_ROOT_IDX,
                                  hist_fbuf, (unsigned long)pos);
}

void history_add(const char *cmd)
{
    if (!cmd || !cmd[0])
        return;

    if (hist_count > 0 &&
        strncmp(hist_entry(hist_count - 1), cmd, HISTORY_ENTRY_MAX) == 0)
        return;

    if (hist_count < HISTORY_MAX)
    {
        s_strlcpy(hist_entry(hist_count), cmd, HISTORY_ENTRY_MAX - 1);
        hist_count++;
    }
    else
    {
        for (int i = 0; i < HISTORY_MAX - 1; i++)
            s_strlcpy(hist_entry(i), hist_entry(i + 1), HISTORY_ENTRY_MAX - 1);
        s_strlcpy(hist_entry(HISTORY_MAX - 1), cmd, HISTORY_ENTRY_MAX - 1);
    }

    history_save();
}

/* Стереть текущий ввод с экрана и из буфера */
void clear_input_line(void)
{
    while (input_len > 0)
    {
        _do_syscall_backspace();
        input_len--;
    }
}

/* Заменить ввод на экране */
void set_input_line(const char *text, size_t len)
{
    clear_input_line();
    size_t copy = (len < INPUT_BUF_SIZE - 1) ? len : INPUT_BUF_SIZE - 1;
    for (size_t i = 0; i < copy; i++)
    {
        unsigned char c = (unsigned char)text[i];
        if (!c)
            break;
        input_buf[input_len++] = (char)c;
        _do_syscall_print_char((unsigned long)c, WHITE);
    }
}

void history_nav_up(void)
{
    if (hist_count == 0)
        return;

    if (hist_idx == -1)
    {
        s_memcpy(hist_save, input_buf, input_len);
        hist_save[input_len] = '\0';
        hist_save_len = (int)input_len;
        hist_idx = hist_count - 1;
    }
    else if (hist_idx > 0)
    {
        hist_idx--;
    }

    const char *e = hist_entry(hist_idx);
    set_input_line(e, s_strlen(e));
}

void history_nav_down(void)
{
    if (hist_idx == -1)
        return;

    if (hist_idx < hist_count - 1)
    {
        hist_idx++;
        const char *e = hist_entry(hist_idx);
        set_input_line(e, s_strlen(e));
    }
    else
    {
        hist_idx = -1;
        set_input_line(hist_save, (size_t)hist_save_len);
    }
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

int _do_syscall_fs_write_file(const char *name, const char *ext, int parent,
                              const void *data, unsigned long size)
{
    long ret;
    register unsigned long _r10 asm("r10") = (unsigned long)(size_t)data;
    register unsigned long _r8 asm("r8") = size;
    asm volatile(
        "int $0x80"
        : "=a"(ret)
        : "a"((unsigned long)SYSCALL_FS_WRITE_FILE_IN_DIR),
          "D"((unsigned long)(size_t)name),
          "S"((unsigned long)(size_t)ext),
          "d"((unsigned long)(unsigned int)parent),
          "r"(_r10), "r"(_r8)
        : "rcx", "r11", "memory");
    return (int)ret;
}

int _do_syscall_fs_read_file(const char *name, const char *ext, int parent,
                             void *buf, unsigned long bufsize,
                             unsigned long *out_size)
{
    long ret;
    register unsigned long _r10 asm("r10") = (unsigned long)(size_t)buf;
    register unsigned long _r8 asm("r8") = bufsize;
    register unsigned long _r9 asm("r9") = (unsigned long)(size_t)out_size;
    asm volatile(
        "int $0x80"
        : "=a"(ret)
        : "a"((unsigned long)SYSCALL_FS_READ_FILE_IN_DIR),
          "D"((unsigned long)(size_t)name),
          "S"((unsigned long)(size_t)ext),
          "d"((unsigned long)(unsigned int)parent),
          "r"(_r10), "r"(_r8), "r"(_r9)
        : "rcx", "r11", "memory");
    return (int)ret;
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