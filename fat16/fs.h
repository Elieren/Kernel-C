// fs.h
#ifndef FS_H
#define FS_H

#include <stdint.h>
#include <stddef.h>

#define FS_NAME_MAX 64
#define FS_EXT_MAX 64
#define FS_MAX_ENTRIES 32768 // файлы + каталоги

#define BYTES_PER_SECTOR 512
#define SECTORS_PER_CLUSTER 128

#define FAT_ENTRIES 600000

/* Индекс корневого каталога в таблице записей */
#define FS_ROOT_IDX 0

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

/* Инициализация файловой системы (вызывает инициализацию FAT и корня) */
void fs_init(void);

int fs_mkdir(const char *name, int parent);
int fs_rmdir(int dir_idx);
int fs_create_file(const char *name, const char *ext, int parent, uint32_t *out_cluster);
int fs_remove_entry(int idx);
int fs_find_in_dir(const char *name, const char *ext, int parent, fs_entry_t *out);
int fs_get_all_in_dir(fs_entry_t *out_files, int max_files, int parent);
size_t fs_read(uint32_t first_cluster, void *buf, size_t size);
size_t fs_write(uint32_t first_cluster, const void *buf, size_t size);
int fs_write_file_in_dir(const char *name, const char *ext, int parent, const void *data, size_t size);
int fs_read_file_in_dir(const char *name, const char *ext, int parent, void *buf, size_t bufsize, size_t *out_size);

#endif // FS_H
