#include "fs.h"
#include "../ramdisk/ramdisk.h"
#include <string.h>
#include <stdint.h>
#include <stddef.h>

#define BYTES_PER_SECTOR 512
#define SECTORS_PER_CLUSTER 128 // 64Кб
#define RESERVED_SECTORS 1
#define NUM_FATS 2
#define SECTORS_PER_FAT 128  // обеспечить поддержку большого количества кластеров

// Размер таблицы FAT
#define FAT_ENTRIES 600000

// Таблица FAT
static uint16_t fat[FAT_ENTRIES];

// Массив записей
static fs_entry_t entries[FS_MAX_ENTRIES];

// Вспомогательные функции
static uint16_t alloc_cluster(void);
static void free_cluster_chain(uint16_t first);
static int nameeq(const char *a, const char *b, size_t n);
static int find_free_entry(void);
static int has_children(int idx);

static uint8_t *get_cluster(uint16_t cluster)
{
    uint32_t first_data_sector = RESERVED_SECTORS + NUM_FATS * SECTORS_PER_FAT;
    return ramdisk_base() + (first_data_sector + (cluster - 2) * SECTORS_PER_CLUSTER) * BYTES_PER_SECTOR;
}

static uint16_t alloc_cluster(void)
{
    for (uint16_t i = 2; i < FAT_ENTRIES; ++i)
    {
        if (fat[i] == 0)
        {
            fat[i] = 0xFFFF; // конец цепочки
            return i;
        }
    }
    return 0;
}

static void free_cluster_chain(uint16_t first)
{
    uint16_t cur = first;
    while (cur >= 2 && cur < FAT_ENTRIES && fat[cur] != 0)
    {
        uint16_t next = fat[cur];
        fat[cur] = 0;
        if (next == 0xFFFF)
            break;
        cur = next;
    }
}

void fs_init(void)
{
    memset(entries, 0, sizeof(entries));
    for (size_t i = 0; i < FAT_ENTRIES; ++i)
        fat[i] = 0;

    // Создаём корень
    entries[FS_ROOT_IDX].used = 1;
    entries[FS_ROOT_IDX].is_dir = 1;
    entries[FS_ROOT_IDX].parent = -1;
    strcpy(entries[FS_ROOT_IDX].name, "/");
    entries[FS_ROOT_IDX].ext[0] = '\0';
    entries[FS_ROOT_IDX].first_cluster = 0;
    entries[FS_ROOT_IDX].size = 0;
}

static int find_free_entry(void)
{
    for (int i = 1; i < FS_MAX_ENTRIES; ++i)
    {
        if (!entries[i].used)
            return i;
    }
    return -1;
}

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

static int nameeq(const char *a, const char *b, size_t n)
{
    for (size_t i = 0; i < n; ++i)
    {
        if (a[i] != b[i])
            return 0;
        if (a[i] == '\0' && b[i] == '\0')
            return 1;
    }
    return 1;
}

int fs_mkdir(const char *name, int parent)
{
    if (!name || parent < 0 || parent >= FS_MAX_ENTRIES)
        return -1;
    if (!entries[parent].used || !entries[parent].is_dir)
        return -2;

    for (int i = 1; i < FS_MAX_ENTRIES; ++i)
    {
        if (entries[i].used && entries[i].parent == parent && entries[i].is_dir)
        {
            if (nameeq(entries[i].name, name, FS_NAME_MAX))
                return -3;
        }
    }

    int idx = find_free_entry();
    if (idx < 0)
        return -4;

    memset(&entries[idx], 0, sizeof(fs_entry_t));
    strncpy(entries[idx].name, name, FS_NAME_MAX - 1);
    entries[idx].name[FS_NAME_MAX - 1] = '\0';
    entries[idx].parent = parent;
    entries[idx].is_dir = 1;
    entries[idx].used = 1;
    entries[idx].first_cluster = 0;
    entries[idx].size = 0;
    return idx;
}

int fs_rmdir(int dir_idx)
{
    if (dir_idx <= 0 || dir_idx >= FS_MAX_ENTRIES)
        return -1;
    if (!entries[dir_idx].used || !entries[dir_idx].is_dir)
        return -2;
    if (has_children(dir_idx))
        return -3;
    entries[dir_idx].used = 0;
    return 0;
}

int fs_create_file(const char *name, const char *ext, int parent, uint16_t *out_cluster)
{
    if (!name || parent < 0 || parent >= FS_MAX_ENTRIES)
        return -1;
    if (!entries[parent].used || !entries[parent].is_dir)
        return -2;

    for (int i = 0; i < FS_MAX_ENTRIES; ++i)
    {
        if (entries[i].used && entries[i].parent == parent && !entries[i].is_dir)
        {
            if (nameeq(entries[i].name, name, FS_NAME_MAX) && nameeq(entries[i].ext, ext, FS_EXT_MAX))
                return -3;
        }
    }

    int idx = find_free_entry();
    if (idx < 0)
        return -4;

    uint16_t c = alloc_cluster();
    if (c == 0)
        return -5;

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

    if (out_cluster)
        *out_cluster = c;

    return idx;
}

int fs_remove_entry(int idx)
{
    if (idx <= 0 || idx >= FS_MAX_ENTRIES)
        return -1;
    if (!entries[idx].used)
        return -2;
    if (entries[idx].is_dir)
    {
        if (has_children(idx))
            return -3;
        entries[idx].used = 0;
        return 0;
    }
    else
    {
        free_cluster_chain(entries[idx].first_cluster);
        entries[idx].used = 0;
        return 0;
    }
}

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
                    continue;
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

int fs_get_all_in_dir(fs_entry_t *out_files, int max_files, int parent)
{
    int count = 0;
    if (parent < 0 || parent >= FS_MAX_ENTRIES)
        return 0;
    for (int i = 0; i < FS_MAX_ENTRIES && count < max_files; ++i)
    {
        if (entries[i].used && entries[i].parent == parent)
        {
            out_files[count] = entries[i];
            if (entries[i].is_dir)
            {
                size_t len = strlen(out_files[count].name);
                if (len < FS_NAME_MAX - 1)
                {
                    out_files[count].name[len] = '/';
                    out_files[count].name[len + 1] = '\0';
                }
            }
            ++count;
        }
    }
    return count;
}

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
        if (fat[cur] == 0xFFFF)
            break;
        cur = fat[cur];
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
            if (fat[cur] == 0 || fat[cur] == 0xFFFF)
            {
                uint16_t nc = alloc_cluster();
                if (nc == 0)
                {
                    fat[cur] = 0xFFFF;
                    return written;
                }
                fat[cur] = nc;
                cur = nc;
            }
            else
            {
                cur = fat[cur];
            }
        }
        else
        {
            fat[cur] = 0xFFFF;
            break;
        }
    }
    return written;
}

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
            return -4;
        idx = cidx;
    }
    else
    {
        uint16_t old = entries[idx].first_cluster;
        free_cluster_chain(old);
        cluster = alloc_cluster();
        if (cluster == 0)
            return -5;
        entries[idx].first_cluster = cluster;
        fat[cluster] = 0xFFFF;
    }

    if (size == 0)
    {
        entries[idx].size = 0;
        fat[entries[idx].first_cluster] = 0xFFFF;
        return 0;
    }

    size_t written = fs_write(entries[idx].first_cluster, data, size);
    if (written != size)
    {
        entries[idx].size = (uint32_t)written;
        return -6;
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
