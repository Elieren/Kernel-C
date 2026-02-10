// fs.h
#ifndef FS_H
#define FS_H

#include <stdint.h>
#include <stddef.h>
#include "drivers/block/ide/ide.h"

#define FS_NAME_MAX 255
#define FS_EXT_MAX 64
#define FS_MAX_ENTRIES 2048 // файлы + каталоги

/* Индекс корневого каталога в таблице записей */
#define FS_ROOT_IDX 0
#define ENTRY_START_IDX 1

/* Общие константы FAT */
#define FAT_FREE 0x0000
#define FAT_EOF 0xFFFF
#define FAT_NO_CLUSTER 0

/* Коды ошибок для функций FS (отрицательные значения) */
#define FS_OK 0
#define FS_ERR_INVALID_ARG -1     // неверные параметры
#define FS_ERR_NOT_DIR -2         // родитель не существует или не каталог
#define FS_ERR_EXISTS -3          // запись с таким именем уже существует
#define FS_ERR_NO_SPACE -4        // нет места в таблице записей
#define FS_ERR_NO_FAT_SPACE -5    // нет места в FAT (нет свободных кластеров)
#define FS_ERR_PARTIAL_WRITE -6   // частично записано
#define FS_ERR_NOT_FOUND -7       // запись не найдена
#define FS_ERR_DISK_IO -8         // ошибка чтения/записи диска
#define FS_ERR_NOT_INITIALIZED -9 // файловая система не инициализирована

#define FS_HAS_CHILDREN 1
#define FS_NO_CHILDREN 0

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

/* Инициализация файловой системы на IDE диске */
int fs_init(ide_disk_t *disk);

/* Форматирование диска (создание новой файловой системы) */
int fs_format(ide_disk_t *disk);

/* Синхронизация (запись кэшированных данных на диск) */
int fs_sync(void);

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
int fs_read(uint16_t first_cluster, void *buf, size_t size);
int fs_write(uint16_t first_cluster, const void *buf, size_t size);

/* Высокоуровневые операции с файлами (по имени + каталогу) */
int fs_write_file_in_dir(const char *name, const char *ext, int parent, const void *data, size_t size);
int fs_read_file_in_dir(const char *name, const char *ext, int parent, void *buf, size_t bufsize, size_t *out_size);

int fs_get_parent_idx(int idx);
int fs_build_path(int idx, char *buf, size_t size);

#endif // FS_H