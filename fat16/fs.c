#include "fs.h"
#include "../ramdisk/ramdisk.h"
#include <string.h>
#include <stdint.h>
#include <stddef.h>

#define BYTES_PER_SECTOR 512
#define SECTORS_PER_CLUSTER 1
#define RESERVED_SECTORS 1
#define NUM_FATS 2
#define SECTORS_PER_FAT 32

#define FAT_ENTRIES (BYTES_PER_SECTOR * SECTORS_PER_FAT)

typedef struct
{
    uint16_t entries[FAT_ENTRIES]; // 0 = свободно, 0xFFFF = EOF, иначе номер следующего кластера
} fat16_table_t;

static fat16_table_t fat;
static fs_entry_t entries[FS_MAX_ENTRIES];

static uint16_t alloc_cluster(void);
static void free_cluster_chain(uint16_t first);
static int nameeq(const char *a, const char *b, size_t n);
static int find_free_entry(void);
static int has_children(int idx);

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
        if (fat.entries[i] == 0)
        {
            fat.entries[i] = 0xFFFF; // помечаем как EOF на момент выделения
            return i;
        }
    }
    return 0; // нет места
}

/* Освободить цепочку кластеров */
static void free_cluster_chain(uint16_t first)
{
    if (first < 2 || first >= FAT_ENTRIES)
        return;
    uint16_t cur = first;
    while (cur != 0 && cur < FAT_ENTRIES && fat.entries[cur] != 0)
    {
        uint16_t next = fat.entries[cur];
        fat.entries[cur] = 0;
        if (next == 0xFFFF)
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
    for (int i = 1; i < FS_MAX_ENTRIES; ++i) // 0 зарезервирован для корня
    {
        if (!entries[i].used)
            return i;
    }
    return -1;
}

/* Проверяет, есть ли у директории дочерние записи */
static int has_children(int idx)
{
    if (idx < 0 || idx >= FS_MAX_ENTRIES)
        return 0;
    for (int i = 0; i < FS_MAX_ENTRIES; ++i)
    {
        if (entries[i].used && entries[i].parent == idx)
            return 1;
    }
    return 0;
}

/* Сравнение имён (безопасно до n символов) */
static int nameeq(const char *a, const char *b, size_t n)
{
    for (size_t i = 0; i < n; ++i)
    {
        char ca = a[i], cb = b[i];
        if (!ca && !cb)
            return 1;
        if (ca != cb)
            return 0;
    }
    return 1;
}

/* Создать директорию */
int fs_mkdir(const char *name, int parent)
{
    if (!name || parent < 0 || parent >= FS_MAX_ENTRIES)
        return -1;
    if (!entries[parent].used || !entries[parent].is_dir)
        return -2; // родитель не существует или не каталог

    // Проверим дубликат
    for (int i = 0; i < FS_MAX_ENTRIES; ++i)
    {
        if (entries[i].used && entries[i].parent == parent && entries[i].is_dir && nameeq(entries[i].name, name, FS_NAME_MAX))
            return -3; // уже существует
    }

    int idx = find_free_entry();
    if (idx < 0)
        return -4; // нет места

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

/* Удалить директорию (по индексу) — только если пуста */
int fs_rmdir(int dir_idx)
{
    if (dir_idx <= 0 || dir_idx >= FS_MAX_ENTRIES)
        return -1;
    if (!entries[dir_idx].used || !entries[dir_idx].is_dir)
        return -2; // не существует или не директория
    if (has_children(dir_idx))
        return -3; // директория не пуста
    entries[dir_idx].used = 0;
    return 0;
}

/* Создать файл в каталоге parent */
int fs_create_file(const char *name, const char *ext, int parent, uint16_t *out_cluster)
{
    if (!name || parent < 0 || parent >= FS_MAX_ENTRIES)
        return -1;
    if (!entries[parent].used || !entries[parent].is_dir)
        return -2;

    // Проверим дубликат
    for (int i = 0; i < FS_MAX_ENTRIES; ++i)
    {
        if (entries[i].used && entries[i].parent == parent && !entries[i].is_dir && nameeq(entries[i].name, name, FS_NAME_MAX) && nameeq(entries[i].ext, ext, FS_EXT_MAX))
            return -3; // уже существует
    }

    int idx = find_free_entry();
    if (idx < 0)
        return -4; // нет места

    uint16_t c = alloc_cluster();
    if (c == 0)
        return -5; // нет места в FAT

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
    fat.entries[c] = 0xFFFF; // пометить EOF до записи
    if (out_cluster)
        *out_cluster = c;
    return idx;
}

/* Удалить запись (файл или пустую директорию) по индексу. Для файлов освобождает кластера */
int fs_remove_entry(int idx)
{
    if (idx <= 0 || idx >= FS_MAX_ENTRIES)
        return -1;
    if (!entries[idx].used)
        return -2;
    if (entries[idx].is_dir)
    {
        if (has_children(idx))
            return -3; // не пустая
        entries[idx].used = 0;
        return 0;
    }
    else
    {
        uint16_t first = entries[idx].first_cluster;
        free_cluster_chain(first);
        entries[idx].used = 0;
        return 0;
    }
}

/* Найти запись по имени/ext в каталоге parent */
int fs_find_in_dir(const char *name, const char *ext, int parent, fs_entry_t *out)
{
    if (!name || parent < 0 || parent >= FS_MAX_ENTRIES)
        return -1;
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
                if (nameeq(entries[i].name, name, FS_NAME_MAX) && nameeq(entries[i].ext, ext ? ext : "", FS_EXT_MAX))
                {
                    if (out)
                        *out = entries[i];
                    return i;
                }
            }
        }
    }
    return -1;
}

/* Получить список файлов/директорий в каталоге parent */
int fs_get_all_in_dir(fs_entry_t *out_files, int max_files, int parent)
{
    int count = 0;
    if (parent < 0 || parent >= FS_MAX_ENTRIES)
        return 0;

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

/* НИЗКОУРОВНЕВЫЕ ЧТЕНИЕ/ЗАПИСЬ*/
size_t fs_read(uint16_t first_cluster, void *buf, size_t size)
{
    if (first_cluster < 2 || first_cluster >= FAT_ENTRIES)
        return 0;
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

        if (fat.entries[cur] == 0xFFFF)
            break;
        cur = fat.entries[cur];
    }

    return read;
}

size_t fs_write(uint16_t first_cluster, const void *buf, size_t size)
{
    uint8_t *data = (uint8_t *)buf;
    size_t cluster_size = BYTES_PER_SECTOR * SECTORS_PER_CLUSTER;

    if (first_cluster < 2 || first_cluster >= FAT_ENTRIES)
        return 0;

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
            if (fat.entries[cur] == 0xFFFF)
            {
                uint16_t nc = alloc_cluster();
                if (nc == 0)
                {
                    fat.entries[cur] = 0xFFFF;
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
            fat.entries[cur] = 0xFFFF;
            break;
        }
    }

    return written;
}

/* Высокоуровневые операции с файлами (по имени в каталоге) */
int fs_write_file_in_dir(const char *name, const char *ext, int parent, const void *data, size_t size)
{
    if (!name)
        return -1;
    if (parent < 0 || parent >= FS_MAX_ENTRIES)
        return -2;
    if (!entries[parent].used || !entries[parent].is_dir)
        return -3;

    fs_entry_t f;
    int idx = fs_find_in_dir(name, ext, parent, &f);
    uint16_t cluster;

    if (idx < 0)
    {
        int cidx = fs_create_file(name, ext, parent, &cluster);
        if (cidx < 0)
            return -4; // ошибка создания
        idx = cidx;
    }
    else
    {
        // файл уже есть — освобождаем старую цепочку и выделяем новый кластер стартовый
        uint16_t old = entries[idx].first_cluster;
        free_cluster_chain(old);
        cluster = alloc_cluster();
        if (cluster == 0)
            return -5; // нет места
        entries[idx].first_cluster = cluster;
        fat.entries[cluster] = 0xFFFF;
    }

    if (size == 0)
    {
        entries[idx].size = 0;
        fat.entries[entries[idx].first_cluster] = 0xFFFF;
        return 0;
    }

    size_t written = fs_write(entries[idx].first_cluster, data, size);
    if (written != size)
    {
        entries[idx].size = (uint32_t)written;
        return -6; // частично записано
    }
    entries[idx].size = (uint32_t)size;
    return 0;
}

int fs_read_file_in_dir(const char *name, const char *ext, int parent, void *buf, size_t bufsize, size_t *out_size)
{
    fs_entry_t f;
    int idx = fs_find_in_dir(name, ext, parent, &f);
    if (idx < 0)
        return -1;
    if (bufsize < f.size)
        return -2;
    size_t r = fs_read(f.first_cluster, buf, f.size);
    if (out_size)
        *out_size = r;
    return 0;
}
