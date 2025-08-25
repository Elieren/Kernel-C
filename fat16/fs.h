// fs.h
#ifndef FS_H
#define FS_H

#include <stdint.h>
#include <stddef.h>

#define FS_NAME_MAX 64
#define FS_EXT_MAX 64
#define FS_MAX_ENTRIES 512 // файлы + каталоги

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

/* Создать директорию с именем name в каталоге parent (индекс). Возвращает индекс новой записи или -1 при ошибке */
int fs_mkdir(const char *name, int parent);

/* Удалить директорию по индексу (директория должна быть пуста). Возвращает 0 — ок*/
int fs_rmdir(int dir_idx);

/* Создать файл в каталоге parent; при успехе возвращает индекс записи >=0 и (опционально) стартовый кластер в out_cluster */
int fs_create_file(const char *name, const char *ext, int parent, uint16_t *out_cluster);

/* Удалить запись (файл или пустую директорию) по индексу. Для файлов освободит кластера. */
int fs_remove_entry(int idx);

/* Найти запись (файл или директорию) по имени/ext в каталоге parent. Возвращает индекс или -1. Если out != NULL, копирует найденную запись туда. */
int fs_find_in_dir(const char *name, const char *ext, int parent, fs_entry_t *out);

/* Получить список всех записей в каталоге parent. Возвращает количество записей, помещённых в out_files (макс = max_files) */
int fs_get_all_in_dir(fs_entry_t *out_files, int max_files, int parent);

/* Прочитать/записать низкоуровневые данные (цепочка кластеров) */
size_t fs_read(uint16_t first_cluster, void *buf, size_t size);
size_t fs_write(uint16_t first_cluster, const void *buf, size_t size);

/* Высокоуровневые операции с файлами (по имени + каталогу) */
int fs_write_file_in_dir(const char *name, const char *ext, int parent, const void *data, size_t size);
int fs_read_file_in_dir(const char *name, const char *ext, int parent, void *buf, size_t bufsize, size_t *out_size);

#endif // FS_H