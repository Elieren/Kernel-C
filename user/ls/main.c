typedef unsigned long size_t;
typedef unsigned long uint64_t;
typedef unsigned int uint32_t;
typedef unsigned char uint8_t;
typedef unsigned long uintptr_t;
typedef short int16_t;
typedef unsigned short uint16_t;

#define SYSCALL_PRINT_STRING 3

#define SYSCALL_GET_CWD_IDX 502
#define SYSCALL_FS_GET_ALL_IN_DIR 605

#define SYSCALL_MALLOC 10
#define SYSCALL_FREE 12

#define SYSCALL_TASK_EXIT 204

#define WHITE 0x00FFFFFF

#define FS_NAME_MAX 255
#define FS_EXT_MAX 64

#define BUF_SIZE 8192

typedef struct
{
    char name[FS_NAME_MAX]; // имя файла или папки (без точки)
    char ext[FS_EXT_MAX];   // расширение для файлов, пусто для директорий
    int16_t parent;         // индекс родительского каталога (FS_ROOT_IDX для корня), -1 для корня
    uint16_t first_cluster; // для файлов: первый кластер, для папок — 0
    uint32_t size;          // размер файла в байтах (0 для директорий)
    uint8_t used;           // 1 — запись занята
    uint8_t is_dir;         // 1 — это директория
} fs_entry_t;

size_t my_strlen(const char *s);
char *my_strcpy(char *dst, const char *src);
char *my_strcat(char *dst, const char *src);
void _do_syscall_print_string(const char *p, unsigned long color);
void *_do_syscall_malloc(unsigned long size);
long _do_syscall_get_cwd_idx(uint32_t *out_idx);
void _do_syscall_free(void *ptr);
int _do_syscall_fs_get_all_in_dir(fs_entry_t *out_files, int max_files, int parent);
void _do_syscall_exit(unsigned long code);

static uint32_t cwd_idx = 0;
static int status = 0;
static char *buffer_ptr = (char *)0;

void _start(void)
{
    buffer_ptr = (char *)_do_syscall_malloc(BUF_SIZE);

    status = _do_syscall_get_cwd_idx(&cwd_idx);
    if (status == -1)
    {
        _do_syscall_free(buffer_ptr);
        _do_syscall_exit(1);
        for (;;)
            asm volatile("pause");
    }

    int max_files = 128;
    size_t entries_size = (size_t)max_files * sizeof(fs_entry_t);
    fs_entry_t *entries = (fs_entry_t *)_do_syscall_malloc(entries_size);
    if (!entries)
    {
        _do_syscall_free(buffer_ptr);
        _do_syscall_exit(1);
        for (;;)
            asm volatile("pause");
    }

    int count = _do_syscall_fs_get_all_in_dir(entries, max_files, (int)cwd_idx);
    if (count < 0)
    {
        _do_syscall_free(entries);
        _do_syscall_free(buffer_ptr);
        _do_syscall_exit(1);
        for (;;)
            asm volatile("pause");
    }

    char *w = buffer_ptr;
    char *end = buffer_ptr + BUF_SIZE;

    /* Заголовок */
    const char *hdr = "Files in current directory:\n";
    for (const char *p = hdr; *p && w < end - 1; ++p)
        *w++ = *p;

    /* Записи */
    for (int i = 0; i < count; ++i)
    {
        const char *label = entries[i].is_dir ? "[DIR] " : "[FILE] ";

        /* Метка типа */
        for (const char *p = label; *p && w < end - 1; ++p)
            *w++ = *p;

        /* Имя */
        for (const char *p = entries[i].name; *p && w < end - 1; ++p)
            *w++ = *p;

        /* Расширение (если файл и есть ненулевая ext) */
        if (!entries[i].is_dir && entries[i].ext[0] != '\0' && w < end - 2)
        {
            /* добавляем точку */
            *w++ = '.';
            for (const char *p = entries[i].ext; *p && w < end - 1; ++p)
                *w++ = *p;
        }

        /* Перевод строки */
        if (w < end - 1)
            *w++ = '\n';
        else
        {
            /* Буфер заполнен - принудительно завершить запись */
            break;
        }
    }

    *w = '\0';

    _do_syscall_print_string(buffer_ptr, WHITE);

    _do_syscall_free(entries);
    _do_syscall_free(buffer_ptr);

    _do_syscall_exit(0);

    for (;;)
        asm volatile("pause");
}

size_t my_strlen(const char *s)
{
    const char *p = s;
    while (*p)
        ++p;
    return (size_t)(p - s);
}
char *my_strcpy(char *dst, const char *src)
{
    char *d = dst;
    while ((*d++ = *src++) != '\0')
    {
    }
    return dst;
}
char *my_strcat(char *dst, const char *src)
{
    char *d = dst;
    while (*d)
        ++d;
    while ((*d++ = *src++) != '\0')
    {
    }
    return dst;
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

int _do_syscall_fs_get_all_in_dir(fs_entry_t *out_files, int max_files, int parent)
{
    int ret;
    asm volatile(
        "int $0x80"
        : "=a"(ret)
        : "a"(SYSCALL_FS_GET_ALL_IN_DIR), "D"(out_files), "S"(max_files), "d"(parent)
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