#ifndef FS_H
#define FS_H

#include <stdint.h>
#include <stddef.h>

#define FS_NAME_MAX 64
#define FS_EXT_MAX 64
#define FS_MAX_FILES 256

typedef struct
{
    char name[FS_NAME_MAX]; // имя файла
    char ext[FS_EXT_MAX];   // расширение
    uint16_t first_cluster; // первый кластер
    uint32_t size;          // размер файла в байтах
    uint8_t used;           // 1 если файл занят
} fs_file_t;

// Инициализация файловой системы
void fs_init(void);

// Создание нового файла
int fs_create(const char *name, const char *ext, uint16_t *out_cluster);

// Чтение данных из файла
size_t fs_read(uint16_t first_cluster, void *buf, size_t size);

// Запись данных в файл
size_t fs_write(uint16_t first_cluster, const void *buf, size_t size);

int fs_find(const char *name, const char *ext, fs_file_t *out);

int fs_write_file(const char *name, const char *ext, const void *data, size_t size);

int fs_read_file(const char *name, const char *ext, void *buf, size_t bufsize, size_t *out_size);

int fs_get_all_files(fs_file_t *out_files, int max_files);

#endif // FS_H
