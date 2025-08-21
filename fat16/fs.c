// fs.c
#include "fs.h"
#include "../ramdisk/ramdisk.h"
#include <string.h>
#include <stdint.h>
#include "../syscall/syscall.h"
#include "../vga/vga.h"

#define BYTES_PER_SECTOR 512
#define SECTORS_PER_CLUSTER 1
#define RESERVED_SECTORS 1
#define NUM_FATS 2
#define SECTORS_PER_FAT 32
#define MAX_ROOT_DIR FS_MAX_FILES

/* Количество записей FAT — оставим как раньше, но вынесем в макрос и используем consistently.
   Это число должно быть согласовано с размером ramdisk. Для простоты используем
   BYTES_PER_SECTOR * SECTORS_PER_FAT как ранее (== 512 * 32 = 16384). */
#define FAT_ENTRIES (BYTES_PER_SECTOR * SECTORS_PER_FAT)

typedef struct
{
    uint16_t entries[FAT_ENTRIES]; // 0 = свободно, 0xFFFF = EOF, иначе номер следующего кластера
} fat16_table_t;

static fat16_table_t fat;
fs_file_t root_dir[MAX_ROOT_DIR];

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

/* Освободить цепочку кластеров, начиная с first (не трогаем root_dir сам по себе) */
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
    memset(root_dir, 0, sizeof(root_dir));
    memset(&fat, 0, sizeof(fat));
}

/* Создать новый файл */
int fs_create(const char *name, const char *ext, uint16_t *out_cluster)
{
    for (int i = 0; i < MAX_ROOT_DIR; i++)
    {
        if (!root_dir[i].used)
        {
            uint16_t c = alloc_cluster();
            if (c == 0)
                return -1; // нет места
            /* strncpy + явная нуль-терминация */
            memset(root_dir[i].name, 0, FS_NAME_MAX + 1);
            memset(root_dir[i].ext, 0, FS_EXT_MAX + 1);
            strncpy(root_dir[i].name, name, FS_NAME_MAX);
            strncpy(root_dir[i].ext, ext, FS_EXT_MAX);
            root_dir[i].first_cluster = c;
            root_dir[i].size = 0;
            root_dir[i].used = 1;
            if (out_cluster)
                *out_cluster = c;
            return i;
        }
    }
    return -1; // нет места в корне
}

/* Чтение данных — следует цепочке кластеров и читает up to size байт */
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

/* Запись данных — перезаписывает файл: освобождаем старую цепочку (если есть),
   выделяем новую цепочку и пишем данные по очереди.
   Возвращаем записанное количество байт (или 0 при ошибке) */
size_t fs_write(uint16_t first_cluster, const void *buf, size_t size)
{
    (void)first_cluster; // мы будем устанавливать новый first_cluster в caller при перезаписи,
                         // но оставим аргумент для совместимости. В fs_write_file мы перезапишем root_dir.first_cluster.
    /* Для простоты эта реализация будет использовать цепочку, начиная с нового выделенного кластера:
       caller (fs_write_file) должен выделить стартовый кластер и записать его в root_dir. */

    // Эта функция реализована как низкоуровневая запись по цепочке:
    // Если caller передаёт first_cluster == 0, попробуем выделить новый.
    uint8_t *data = (uint8_t *)buf;
    size_t cluster_size = BYTES_PER_SECTOR * SECTORS_PER_CLUSTER;

    // Если caller дал некорректный кластер — считаем ошибкой.
    if (first_cluster < 2 || first_cluster >= FAT_ENTRIES)
        return 0;

    uint16_t cur = first_cluster;
    size_t written = 0;

    while (written < size)
    {
        // если cluster занят, просто переписываем его; если по какой-то причине fat.entries[cur] == 0, пометим EOF
        uint8_t *clptr = get_cluster(cur);
        size_t to_write = size - written;
        if (to_write > cluster_size)
            to_write = cluster_size;
        memcpy(clptr, data + written, to_write);
        written += to_write;

        if (written < size)
        {
            // нужно следующий кластер
            if (fat.entries[cur] == 0xFFFF)
            {
                uint16_t nc = alloc_cluster();
                if (nc == 0)
                {
                    // нет места — ставим EOF и возвращаем то, что записали
                    fat.entries[cur] = 0xFFFF;
                    return written;
                }
                fat.entries[cur] = nc;
                cur = nc;
            }
            else
            {
                // уже есть следующий в цепочке (например, файл раньше был длиннее) — используем его
                cur = fat.entries[cur];
            }
        }
        else
        {
            // дошли до конца записи — пометить текущий как EOF
            fat.entries[cur] = 0xFFFF;
            break;
        }
    }

    return written;
}

/* Сравнение имён (как раньше) */
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

int fs_find(const char *name, const char *ext, fs_file_t *out)
{
    for (int i = 0; i < MAX_ROOT_DIR; ++i)
    {
        if (root_dir[i].used &&
            nameeq(root_dir[i].name, name, FS_NAME_MAX) &&
            nameeq(root_dir[i].ext, ext, FS_EXT_MAX))
        {
            if (out)
                *out = root_dir[i];
            return i;
        }
    }
    return -1;
}

/* Высокоуровневая запись файла: перезаписывает существующий файл (освобождает старую цепочку),
   выделяет новую цепочку и записывает данные (любой длины). */
int fs_write_file(const char *name, const char *ext, const void *data, size_t size)
{
    uint16_t cluster;
    int idx = fs_find(name, ext, NULL);
    if (idx < 0)
    {
        idx = fs_create(name, ext, &cluster);
        if (idx < 0)
            return -1;
    }
    else
    {
        // если файл уже есть — освободим старую цепочку и выделим новый стартовый кластер
        uint16_t old = root_dir[idx].first_cluster;
        free_cluster_chain(old);
        cluster = alloc_cluster();
        if (cluster == 0)
            return -3; // нет места
        root_dir[idx].first_cluster = cluster;
    }

    // Если размер 0 — просто установим size и пометим EOF в единственном кластере
    if (size == 0)
    {
        root_dir[idx].size = 0;
        fat.entries[cluster] = 0xFFFF;
        return 0;
    }

    // Если данных больше чем один кластер, fs_write сам добавит новые кластера (через alloc_cluster)
    size_t written = fs_write(cluster, data, size);
    if (written != size)
    {
        // частично записано или ошибка — можно вернуть код ошибки
        root_dir[idx].size = (uint32_t)written;
        return -2;
    }

    root_dir[idx].size = (uint32_t)size;
    return 0;
}

int fs_read_file(const char *name, const char *ext, void *buf, size_t bufsize, size_t *out_size)
{
    fs_file_t f;
    int idx = fs_find(name, ext, &f);
    if (idx < 0)
        return -1;
    if (bufsize < f.size)
        return -2;
    size_t r = fs_read(f.first_cluster, buf, f.size);
    if (out_size)
        *out_size = r;
    return 0;
}

int fs_get_all_files(fs_file_t *out_files, int max_files)
{
    int count = 0;
    for (int i = 0; i < MAX_ROOT_DIR && count < max_files; ++i)
    {
        if (root_dir[i].used)
        {
            out_files[count++] = root_dir[i];
        }
    }
    return count; // количество найденных файлов
}