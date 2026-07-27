// cat.c – вывод содержимого файла

typedef unsigned long size_t;
typedef unsigned long uint64_t;
typedef unsigned int uint32_t;
typedef unsigned char uint8_t;
typedef unsigned long uintptr_t;
typedef short int16_t;
typedef unsigned short uint16_t;

#define NULL ((void *)0)

// syscalls
#define SYSCALL_PRINT_CHAR 2
#define SYSCALL_PRINT_STRING 3
#define SYSCALL_MALLOC 10
#define SYSCALL_FREE 12
#define SYSCALL_TASK_EXIT 204
#define SYSCALL_GET_CWD_IDX 502
#define SYSCALL_FS_FIND_IN_DIR 604
#define SYSCALL_FS_READ 606

#define WHITE 0x00FFFFFFu
#define RED 0x00FF0000u

#define FS_NAME_MAX 255
#define FS_EXT_MAX 64

typedef struct
{
    char name[FS_NAME_MAX];
    char ext[FS_EXT_MAX];
    int16_t parent;
    uint16_t first_cluster;
    uint32_t size;
    uint8_t used;
    uint8_t is_dir;
} fs_entry_t;

void _do_syscall_print_string(const char *p, unsigned long color);
void _do_syscall_print_char(char c, unsigned long color);
void *_do_syscall_malloc(unsigned long size);
void _do_syscall_free(void *ptr);
long _do_syscall_get_cwd_idx(uint32_t *out_idx);
int _do_syscall_fs_find_in_dir(const char *name, const char *ext, int parent, fs_entry_t *out);
int _do_syscall_fs_read(uint16_t first_cluster, void *buf, size_t size);
void _do_syscall_exit(unsigned long code);

size_t my_strlen(const char *s);
int my_strcmp(const char *s1, const char *s2);

// печатает msg (если не NULL) и завершает процесс
static void die(const char *msg, unsigned long code)
{
    if (msg)
        _do_syscall_print_string(msg, RED);
    _do_syscall_exit(code);
    for (;;)
        asm volatile("pause");
}

// разбивает path на имя/расширение по последней точке
static const char *split_name_ext(const char *path, char *name_buf, char *ext_buf)
{
    size_t len = my_strlen(path);
    if (len >= FS_NAME_MAX)
        len = FS_NAME_MAX - 1;

    int dot_pos = -1;
    for (int i = (int)len - 1; i >= 0; --i)
    {
        if (path[i] == '.')
        {
            dot_pos = i;
            break;
        }
    }

    // ".bashrc" – не расширение
    if (dot_pos <= 0)
    {
        for (size_t i = 0; i < len; ++i)
            name_buf[i] = path[i];
        name_buf[len] = '\0';
        return NULL;
    }

    size_t name_len = (size_t)dot_pos;
    if (name_len >= FS_NAME_MAX)
        name_len = FS_NAME_MAX - 1;
    for (size_t i = 0; i < name_len; ++i)
        name_buf[i] = path[i];
    name_buf[name_len] = '\0';

    size_t ext_len = len - (size_t)dot_pos - 1;
    if (ext_len >= FS_EXT_MAX)
        ext_len = FS_EXT_MAX - 1;
    for (size_t i = 0; i < ext_len; ++i)
        ext_buf[i] = path[dot_pos + 1 + i];
    ext_buf[ext_len] = '\0';

    return ext_buf;
}

void _start(int argc, char **argv)
{
    if (argc < 2)
        die("Usage: cat <file>\n", 1);

    const char *target = argv[1];

    uint32_t cwd_idx = 0;
    if (_do_syscall_get_cwd_idx(&cwd_idx) != 0)
        die("Error: cannot get current directory\n", 1);

    char name_buf[FS_NAME_MAX];
    char ext_buf[FS_EXT_MAX];
    const char *ext_ptr = split_name_ext(target, name_buf, ext_buf);

    fs_entry_t entry;
    if (_do_syscall_fs_find_in_dir(name_buf, ext_ptr, (int)cwd_idx, &entry) < 0)
    {
        _do_syscall_print_string("Error: file '", RED);
        _do_syscall_print_string(target, RED);
        die("' not found\n", 1);
    }

    if (entry.size == 0)
        die(NULL, 0);

    void *buf = _do_syscall_malloc(entry.size);
    if (!buf)
        die("Error: out of memory\n", 1);

    int bytes_read = _do_syscall_fs_read(entry.first_cluster, buf, entry.size);
    if (bytes_read < 0 || (size_t)bytes_read != entry.size)
    {
        _do_syscall_free(buf);
        _do_syscall_print_string("Error: cannot read file '", RED);
        _do_syscall_print_string(target, RED);
        die("'\n", 1);
    }

    const unsigned char *p = (const unsigned char *)buf;
    for (size_t i = 0; i < entry.size; ++i)
        _do_syscall_print_char((char)p[i], WHITE);
    _do_syscall_print_char('\n', WHITE);

    _do_syscall_free(buf);
    die(NULL, 0);
}

// ---- syscalls ----

void _do_syscall_print_string(const char *p, unsigned long color)
{
    asm volatile(
        "int $0x80"
        :
        : "a"(SYSCALL_PRINT_STRING), "D"(p), "S"(color)
        : "rcx", "r11", "memory");
}

void _do_syscall_print_char(char c, unsigned long color)
{
    asm volatile(
        "int $0x80"
        :
        : "a"(SYSCALL_PRINT_CHAR), "D"((unsigned long)c), "S"(color)
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

int _do_syscall_fs_find_in_dir(const char *name, const char *ext, int parent, fs_entry_t *out)
{
    int ret;
    register uint64_t r10_reg __asm__("r10") = (uint64_t)out;
    asm volatile(
        "int $0x80"
        : "=a"(ret), "+r"(r10_reg)
        : "a"(SYSCALL_FS_FIND_IN_DIR), "D"(name), "S"(ext), "d"(parent)
        : "rcx", "r11", "memory");
    return ret;
}

int _do_syscall_fs_read(uint16_t first_cluster, void *buf, size_t size)
{
    int ret;
    asm volatile(
        "int $0x80"
        : "=a"(ret)
        : "a"(SYSCALL_FS_READ), "D"((unsigned long)first_cluster), "S"(buf), "d"(size)
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

// ---- utils ----

size_t my_strlen(const char *s)
{
    const char *p = s;
    while (*p)
        ++p;
    return (size_t)(p - s);
}

int my_strcmp(const char *s1, const char *s2)
{
    while (*s1 && (*s1 == *s2))
    {
        s1++;
        s2++;
    }
    return *(const unsigned char *)s1 - *(const unsigned char *)s2;
}