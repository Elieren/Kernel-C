typedef unsigned long size_t;
typedef unsigned long uint64_t;
typedef unsigned int uint32_t;
typedef unsigned char uint8_t;
typedef unsigned long uintptr_t;
typedef short int16_t;
typedef unsigned short uint16_t;

#define NULL ((void *)0)

#define SYSCALL_PRINT_STRING 3
#define SYSCALL_MALLOC 10
#define SYSCALL_FREE 12
#define SYSCALL_TASK_EXIT 204
#define SYSCALL_GET_CWD_IDX 502
#define SYSCALL_FS_REMOVE_ENTRY 603
#define SYSCALL_FS_FIND_IN_DIR 604
#define SYSCALL_FS_GET_ALL_IN_DIR 605

#define WHITE 0x00FFFFFF
#define RED 0x00FF0000

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

// Прототипы функций
void _do_syscall_print_string(const char *p, unsigned long color);
void *_do_syscall_malloc(unsigned long size);
void _do_syscall_free(void *ptr);
long _do_syscall_get_cwd_idx(uint32_t *out_idx);
int _do_syscall_fs_remove_entry(int idx);
int _do_syscall_fs_find_in_dir(const char *name, const char *ext, int parent, fs_entry_t *out);
int _do_syscall_fs_get_all_in_dir(fs_entry_t *out_files, int max_files, int parent);
void _do_syscall_exit(unsigned long code);

size_t my_strlen(const char *s);
int my_strcmp(const char *s1, const char *s2);
int remove_recursive(int dir_idx);

void _start(int argc, char **argv)
{
    int recursive = 0;
    const char *target = NULL;

    // Парсинг аргументов
    if (argc < 2)
    {
        _do_syscall_print_string("Usage: rm [-r] <file_or_directory>\n", RED);
        _do_syscall_exit(1);
        for (;;)
            asm volatile("hlt");
    }

    // Проверяем наличие флага -r
    int arg_idx = 1;
    if (argc >= 2 && my_strcmp(argv[1], "-r") == 0)
    {
        recursive = 1;
        arg_idx = 2;
    }

    if (arg_idx >= argc)
    {
        _do_syscall_print_string("Error: no file or directory specified\n", RED);
        _do_syscall_exit(1);
        for (;;)
            asm volatile("hlt");
    }

    target = argv[arg_idx];

    // Получаем текущую директорию
    uint32_t cwd_idx = 0;
    long status = _do_syscall_get_cwd_idx(&cwd_idx);
    if (status != 0)
    {
        _do_syscall_print_string("Error: cannot get current directory\n", RED);
        _do_syscall_exit(1);
        for (;;)
            asm volatile("hlt");
    }

    // Ищем целевой файл/директорию
    fs_entry_t entry;
    int found_idx = _do_syscall_fs_find_in_dir(target, (const char *)0, (int)cwd_idx, &entry);

    if (found_idx < 0)
    {
        _do_syscall_print_string("Error: '", RED);
        _do_syscall_print_string(target, RED);
        _do_syscall_print_string("' not found\n", RED);
        _do_syscall_exit(1);
        for (;;)
            asm volatile("hlt");
    }

    // Если это директория и не указан флаг -r
    if (entry.is_dir && !recursive)
    {
        _do_syscall_print_string("Error: '", RED);
        _do_syscall_print_string(target, RED);
        _do_syscall_print_string("' is a directory. Use -r to remove directories\n", RED);
        _do_syscall_exit(1);
        for (;;)
            asm volatile("hlt");
    }

    // Удаляем
    int result;
    if (entry.is_dir && recursive)
    {
        result = remove_recursive(found_idx);
    }
    else
    {
        result = _do_syscall_fs_remove_entry(found_idx);
    }

    if (result < 0)
    {
        _do_syscall_print_string("Error: cannot remove '", RED);
        _do_syscall_print_string(target, RED);
        _do_syscall_print_string("'\n", RED);
        _do_syscall_exit(1);
        for (;;)
            asm volatile("hlt");
    }

    _do_syscall_exit(0);
    for (;;)
        asm volatile("hlt");
}

// Рекурсивное удаление директории
int remove_recursive(int dir_idx)
{
    // Получаем список всех файлов в директории
    int max_files = 128;
    size_t entries_size = (size_t)max_files * sizeof(fs_entry_t);
    fs_entry_t *entries = (fs_entry_t *)_do_syscall_malloc(entries_size);
    if (!entries)
        return -1;

    int count = _do_syscall_fs_get_all_in_dir(entries, max_files, dir_idx);
    if (count < 0)
    {
        _do_syscall_free(entries);
        return -1;
    }

    // Удаляем все файлы и поддиректории
    for (int i = 0; i < count; i++)
    {
        // Убираем '/' из имени директории если есть
        size_t len = my_strlen(entries[i].name);
        if (len > 0 && entries[i].name[len - 1] == '/')
            entries[i].name[len - 1] = '\0';

        // Находим индекс записи
        fs_entry_t temp;
        const char *ext_param = entries[i].is_dir ? (const char *)0 : entries[i].ext;
        int idx = _do_syscall_fs_find_in_dir(entries[i].name,
                                             ext_param,
                                             dir_idx,
                                             &temp);
        if (idx < 0)
            continue;

        if (entries[i].is_dir)
        {
            // Рекурсивно удаляем поддиректорию
            int res = remove_recursive(idx);
            if (res < 0)
            {
                _do_syscall_free(entries);
                return -1;
            }
        }
        else
        {
            // Удаляем файл
            int res = _do_syscall_fs_remove_entry(idx);
            if (res < 0)
            {
                _do_syscall_free(entries);
                return -1;
            }
        }
    }

    _do_syscall_free(entries);

    // Удаляем саму директорию (теперь пустую)
    return _do_syscall_fs_remove_entry(dir_idx);
}

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

int _do_syscall_fs_remove_entry(int idx)
{
    int ret;
    asm volatile(
        "int $0x80"
        : "=a"(ret)
        : "a"(SYSCALL_FS_REMOVE_ENTRY), "D"(idx)
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