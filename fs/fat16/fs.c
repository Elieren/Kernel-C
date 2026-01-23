#include "fs.h"
#include "mm/ramdisk/ramdisk.h"
#include "lib/string/string.h"
#include <stdint.h>
#include <stddef.h>

#define BYTES_PER_SECTOR 8192
#define SECTORS_PER_CLUSTER 64
#define RESERVED_SECTORS 1
#define NUM_FATS 2
#define SECTORS_PER_FAT 32

#define FAT_ENTRIES ((BYTES_PER_SECTOR * SECTORS_PER_FAT) / sizeof(uint16_t))

typedef struct
{
    uint16_t entries[FAT_ENTRIES]; // 0 = свободно, FAT_EOF = EOF, иначе номер следующего кластера
} fat16_table_t;

static fat16_table_t fat;
static fs_entry_t entries[FS_MAX_ENTRIES];

/* Возвращает указатель на кластер */
uint8_t *get_cluster(uint16_t cluster)
{
    uint8_t *base = ramdisk_base();
    uint32_t first_data_sector = RESERVED_SECTORS + NUM_FATS * SECTORS_PER_FAT;
    return base + first_data_sector * BYTES_PER_SECTOR + (cluster - 2) * SECTORS_PER_CLUSTER * BYTES_PER_SECTOR;
}

/* Найти свободный кластер */
static uint16_t alloc_cluster(void)
{
    for (uint16_t i = 2; i < FAT_ENTRIES; i++)
    {
        if (fat.entries[i] == FAT_FREE)
        {
            fat.entries[i] = FAT_EOF;
            return i;
        }
    }
    return FAT_NO_CLUSTER;
}

/* Освободить цепочку кластеров */
static void free_cluster_chain(uint16_t first)
{
    if (first < 2 || first >= FAT_ENTRIES)
        return;
    uint16_t cur = first;
    while (cur != 0 && cur < FAT_ENTRIES && fat.entries[cur] != FAT_FREE)
    {
        uint16_t next = fat.entries[cur];
        fat.entries[cur] = FAT_FREE;
        if (next == FAT_EOF)
            break;
        cur = next;
    }
}

/* Инициализация FS */
void fs_init(void)
{
    memset(entries, 0, sizeof(entries));
    memset(&fat, 0, sizeof(fat));

    /* Создадим запись корня */
    entries[FS_ROOT_IDX].used = 1;
    entries[FS_ROOT_IDX].is_dir = 1;
    entries[FS_ROOT_IDX].parent = -1;
    strncpy(entries[FS_ROOT_IDX].name, "/", FS_NAME_MAX - 1);
    entries[FS_ROOT_IDX].name[FS_NAME_MAX - 1] = '\0';
    entries[FS_ROOT_IDX].ext[0] = '\0';
    entries[FS_ROOT_IDX].first_cluster = 0;
    entries[FS_ROOT_IDX].size = 0;
}

/* Найти свободную запись в таблице */
static int find_free_entry(void)
{
    for (int i = ENTRY_START_IDX; i < FS_MAX_ENTRIES; ++i) // 0 зарезервирован для корня
    {
        if (!entries[i].used)
            return i;
    }
    return FS_ERR_NO_SPACE;
}

/* Проверяет, есть ли у директории дочерние записи */
static int has_children(int idx)
{
    if (idx < 0 || idx >= FS_MAX_ENTRIES)
        return FS_NO_CHILDREN;
    for (int i = 0; i < FS_MAX_ENTRIES; ++i)
    {
        if (entries[i].used && entries[i].parent == idx)
            return FS_HAS_CHILDREN;
    }
    return FS_NO_CHILDREN;
}

/* Создать директорию */
int fs_mkdir(const char *name, int parent)
{
    if (!name || parent < 0 || parent >= FS_MAX_ENTRIES)
        return FS_ERR_INVALID_ARG;
    if (!entries[parent].used || !entries[parent].is_dir)
        return FS_ERR_NOT_DIR;

    // Проверим дубликат
    for (int i = 0; i < FS_MAX_ENTRIES; ++i)
    {
        if (entries[i].used && entries[i].parent == parent && entries[i].is_dir && nameeq(entries[i].name, name, FS_NAME_MAX))
            return FS_ERR_EXISTS;
    }

    int idx = find_free_entry();
    if (idx < 0)
        return FS_ERR_NO_SPACE;

    memset(&entries[idx], 0, sizeof(fs_entry_t));
    strncpy(entries[idx].name, name, FS_NAME_MAX - 1);
    entries[idx].name[FS_NAME_MAX - 1] = '\0';
    entries[idx].ext[0] = '\0';
    entries[idx].parent = parent;
    entries[idx].is_dir = 1;
    entries[idx].used = 1;
    entries[idx].first_cluster = 0;
    entries[idx].size = 0;
    return idx;
}

/* Удалить директорию (по индексу) - только если пуста */
int fs_rmdir(int dir_idx)
{
    if (dir_idx <= 0 || dir_idx >= FS_MAX_ENTRIES)
        return FS_ERR_INVALID_ARG;
    if (!entries[dir_idx].used || !entries[dir_idx].is_dir)
        return FS_ERR_NOT_DIR;
    if (has_children(dir_idx))
        return FS_ERR_EXISTS;
    entries[dir_idx].used = 0;
    return FS_OK;
}

/* Создать файл в каталоге parent */
int fs_create_file(const char *name, const char *ext, int parent, uint16_t *out_cluster)
{
    if (!name || parent < 0 || parent >= FS_MAX_ENTRIES)
        return FS_ERR_INVALID_ARG;
    if (!entries[parent].used || !entries[parent].is_dir)
        return FS_ERR_NOT_DIR;

    // Проверим дубликат
    for (int i = 0; i < FS_MAX_ENTRIES; ++i)
    {
        if (entries[i].used && entries[i].parent == parent && !entries[i].is_dir &&
            nameeq(entries[i].name, name, FS_NAME_MAX) &&
            nameeq(entries[i].ext, ext, FS_EXT_MAX))
            return FS_ERR_EXISTS;
    }

    int idx = find_free_entry();
    if (idx < 0)
        return FS_ERR_NO_SPACE;

    uint16_t c = alloc_cluster();
    if (c == FAT_NO_CLUSTER)
        return FS_ERR_NO_FAT_SPACE;

    memset(&entries[idx], 0, sizeof(fs_entry_t));
    strncpy(entries[idx].name, name, FS_NAME_MAX - 1);
    entries[idx].name[FS_NAME_MAX - 1] = '\0';
    if (ext)
        strncpy(entries[idx].ext, ext, FS_EXT_MAX - 1);
    entries[idx].ext[FS_EXT_MAX - 1] = '\0';
    entries[idx].parent = parent;
    entries[idx].is_dir = 0;
    entries[idx].used = 1;
    entries[idx].first_cluster = c;
    entries[idx].size = 0;
    fat.entries[c] = FAT_EOF; // пометить EOF до записи
    if (out_cluster)
        *out_cluster = c;
    return idx;
}

/* Удалить запись (файл или пустую директорию) по индексу. Для файлов освобождает кластера */
int fs_remove_entry(int idx)
{
    if (idx <= 0 || idx >= FS_MAX_ENTRIES)
        return FS_ERR_INVALID_ARG;
    if (!entries[idx].used)
        return FS_ERR_NOT_DIR;
    if (entries[idx].is_dir)
    {
        if (has_children(idx))
            return FS_ERR_EXISTS;
        entries[idx].used = 0;
        return FS_OK;
    }
    else
    {
        uint16_t first = entries[idx].first_cluster;
        free_cluster_chain(first);
        entries[idx].used = 0;
        return FS_OK;
    }
}

/* Найти запись по имени/ext в каталоге parent */
int fs_find_in_dir(const char *name, const char *ext, int parent, fs_entry_t *out)
{
    if (!name || parent < 0 || parent >= FS_MAX_ENTRIES)
        return FS_ERR_INVALID_ARG;
    for (int i = 0; i < FS_MAX_ENTRIES; ++i)
    {
        if (entries[i].used && entries[i].parent == parent)
        {
            if (entries[i].is_dir)
            {
                if (ext && ext[0] != '\0')
                    continue; // искали файл, а это директория
                if (nameeq(entries[i].name, name, FS_NAME_MAX))
                {
                    if (out)
                        *out = entries[i];
                    return i;
                }
            }
            else
            {
                if (nameeq(entries[i].name, name, FS_NAME_MAX) &&
                    nameeq(entries[i].ext, ext ? ext : "", FS_EXT_MAX))
                {
                    if (out)
                        *out = entries[i];
                    return i;
                }
            }
        }
    }
    return FS_ERR_NOT_FOUND;
}

/* Получить список файлов/директорий в каталоге parent */
int fs_get_all_in_dir(fs_entry_t *out_files, int max_files, int parent)
{
    int count = 0;
    if (parent < 0 || parent >= FS_MAX_ENTRIES)
        return FS_ERR_INVALID_ARG;

    for (int i = 0; i < FS_MAX_ENTRIES && count < max_files; ++i)
    {
        if (entries[i].used && entries[i].parent == parent)
        {
            out_files[count] = entries[i]; // копируем запись

            // Для директорий добавляем '/' только в копии
            if (out_files[count].is_dir)
            {
                size_t len = strlen(out_files[count].name);
                if (len < FS_NAME_MAX - 1)
                {
                    out_files[count].name[len] = '/';
                    out_files[count].name[len + 1] = '\0';
                }
            }

            count++;
        }
    }
    return count;
}

/* НИЗКОУРОВНЕВЫЕ ЧТЕНИЕ/ЗАПИСЬ */
int fs_read(uint16_t first_cluster, void *buf, size_t size)
{
    if (first_cluster < 2 || first_cluster >= FAT_ENTRIES)
        return FS_ERR_INVALID_ARG;
    size_t cluster_size = BYTES_PER_SECTOR * SECTORS_PER_CLUSTER;
    uint16_t cur = first_cluster;
    size_t read = 0;
    uint8_t *out = (uint8_t *)buf;

    while (cur != 0 && cur < FAT_ENTRIES && read < size)
    {
        uint8_t *clptr = get_cluster(cur);
        size_t to_copy = size - read;
        if (to_copy > cluster_size)
            to_copy = cluster_size;
        memcpy(out + read, clptr, to_copy);
        read += to_copy;

        if (fat.entries[cur] == FAT_EOF)
            break;
        cur = fat.entries[cur];
    }

    return read;
}

int fs_write(uint16_t first_cluster, const void *buf, size_t size)
{
    uint8_t *data = (uint8_t *)buf;
    size_t cluster_size = BYTES_PER_SECTOR * SECTORS_PER_CLUSTER;

    if (first_cluster < 2 || first_cluster >= FAT_ENTRIES)
        return FS_ERR_INVALID_ARG;

    uint16_t cur = first_cluster;
    size_t written = 0;

    while (written < size)
    {
        uint8_t *clptr = get_cluster(cur);
        size_t to_write = size - written;
        if (to_write > cluster_size)
            to_write = cluster_size;
        memcpy(clptr, data + written, to_write);
        written += to_write;

        if (written < size)
        {
            if (fat.entries[cur] == FAT_EOF)
            {
                uint16_t nc = alloc_cluster();
                if (nc == FAT_NO_CLUSTER)
                {
                    fat.entries[cur] = FAT_EOF;
                    return written;
                }
                fat.entries[cur] = nc;
                cur = nc;
            }
            else
            {
                cur = fat.entries[cur];
            }
        }
        else
        {
            fat.entries[cur] = FAT_EOF;
            break;
        }
    }

    return written;
}

/* Высокоуровневые операции с файлами (по имени в каталоге) */
int fs_write_file_in_dir(const char *name, const char *ext, int parent, const void *data, size_t size)
{
    if (!name)
        return FS_ERR_INVALID_ARG;
    if (parent < 0 || parent >= FS_MAX_ENTRIES)
        return FS_ERR_INVALID_ARG;
    if (!entries[parent].used || !entries[parent].is_dir)
        return FS_ERR_NOT_DIR;

    fs_entry_t f;
    int idx = fs_find_in_dir(name, ext, parent, &f);
    uint16_t cluster;

    if (idx < 0)
    {
        int cidx = fs_create_file(name, ext, parent, &cluster);
        if (cidx < 0)
            return cidx; // возвращаем код ошибки создания
        idx = cidx;
    }
    else
    {
        // файл уже есть - освобождаем старую цепочку и выделяем новый кластер стартовый
        uint16_t old = entries[idx].first_cluster;
        free_cluster_chain(old);
        cluster = alloc_cluster();
        if (cluster == FAT_NO_CLUSTER)
            return FS_ERR_NO_FAT_SPACE; // нет места
        entries[idx].first_cluster = cluster;
        fat.entries[cluster] = FAT_EOF;
    }

    if (size == 0)
    {
        entries[idx].size = 0;
        fat.entries[entries[idx].first_cluster] = FAT_EOF;
        return FS_OK;
    }

    int written = fs_write(entries[idx].first_cluster, data, size);
    if (written < 0)
        return written;

    if ((size_t)written != size)
    {
        entries[idx].size = (uint32_t)written;
        return FS_ERR_PARTIAL_WRITE; // частично записано
    }
    entries[idx].size = (uint32_t)size;
    return FS_OK;
}

int fs_read_file_in_dir(const char *name, const char *ext, int parent, void *buf, size_t bufsize, size_t *out_size)
{
    if (!name || !buf)
        return FS_ERR_INVALID_ARG;

    fs_entry_t f;
    int idx = fs_find_in_dir(name, ext, parent, &f);
    if (idx < 0)
        return FS_ERR_NOT_FOUND;
    if (bufsize < f.size)
        return FS_ERR_NO_SPACE;
    int r = fs_read(f.first_cluster, buf, f.size);
    if (r < 0)
        return r;

    if (out_size)
        *out_size = r;
    return FS_OK;
}

int fs_get_parent_idx(int idx)
{
    if (idx < 0 || idx >= FS_MAX_ENTRIES)
        return FS_ERR_INVALID_ARG;
    if (!entries[idx].used)
        return FS_ERR_INVALID_ARG;
    return entries[idx].parent;
}

int fs_build_path(int idx, char *buf, size_t size)
{
    if (!buf || size == 0)
        return 0;
    if (idx < 0 || idx >= FS_MAX_ENTRIES)
        return 0;
    if (!entries[idx].used)
        return 0;

    if (idx == FS_ROOT_IDX)
    {
        if (size < 2)
            return 0;
        buf[0] = '/';
        buf[1] = '\0';
        return 1;
    }

    int stack[FS_MAX_ENTRIES];
    int top = 0;
    int cur = idx;
    while (cur != FS_ROOT_IDX && cur >= 0 && cur < FS_MAX_ENTRIES && entries[cur].used)
    {
        stack[top++] = cur;
        int p = entries[cur].parent;
        if (p == -1)
            break;
        cur = p;
        if (top >= FS_MAX_ENTRIES)
            break;
    }

    size_t sum = 0;
    for (int i = 0; i < top; ++i)
        sum += strlen(entries[stack[i]].name);
    size_t required = sum + (size_t)top + 1;
    if (required > size)
        return 0;

    size_t pos = 0;
    for (int i = top - 1; i >= 0; --i)
    {
        buf[pos++] = '/';
        const char *n = entries[stack[i]].name;
        size_t ln = strlen(n);
        memcpy(buf + pos, n, ln);
        pos += ln;
    }
    buf[pos] = '\0';
    return 1;
}