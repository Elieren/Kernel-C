#include "fs.h"
#include "lib/string/string.h"
#include <stdint.h>
#include <stddef.h>

/* Параметры файловой системы */
#define BYTES_PER_SECTOR 512  // Размер сектора IDE диска
#define SECTORS_PER_CLUSTER 8 // 8 * 512 = 4KB на кластер
#define RESERVED_SECTORS 1    // Загрузочный сектор
#define NUM_FATS 2            // Две копии FAT для надёжности
#define SECTORS_PER_FAT 32    // 32 сектора на FAT

/* Вычисляемые параметры */
#define BYTES_PER_CLUSTER (BYTES_PER_SECTOR * SECTORS_PER_CLUSTER)
#define FAT_ENTRIES ((BYTES_PER_SECTOR * SECTORS_PER_FAT) / sizeof(uint16_t))

/* Расположение структур на диске (в секторах) */
#define BOOT_SECTOR_LBA 0
#define FAT1_START_LBA (RESERVED_SECTORS)
#define FAT2_START_LBA (FAT1_START_LBA + SECTORS_PER_FAT)
#define ENTRIES_TABLE_START_LBA (FAT2_START_LBA + SECTORS_PER_FAT)
#define ENTRIES_TABLE_SECTORS ((sizeof(fs_entry_t) * FS_MAX_ENTRIES + BYTES_PER_SECTOR - 1) / BYTES_PER_SECTOR)
#define DATA_START_LBA (ENTRIES_TABLE_START_LBA + ENTRIES_TABLE_SECTORS)

/* Сигнатура файловой системы */
#define FS_SIGNATURE 0x4653314C // "FS1L"

typedef struct
{
    uint16_t entries[FAT_ENTRIES]; // 0 = свободно, FAT_EOF = EOF, иначе номер следующего кластера
} fat16_table_t;

typedef struct
{
    uint32_t signature;          // FS_SIGNATURE
    uint32_t total_sectors;      // Общее количество секторов ФС
    uint16_t bytes_per_sector;   // Байт на сектор
    uint8_t sectors_per_cluster; // Секторов на кластер
    uint8_t reserved_sectors;    // Зарезервированные сектора
    uint8_t num_fats;            // Количество FAT таблиц
    uint8_t padding[501];        // Заполнение до 512 байт
} __attribute__((packed)) fs_boot_sector_t;

/* Глобальные переменные */
static ide_disk_t *g_disk = NULL;
static fat16_table_t fat;
static fs_entry_t entries[FS_MAX_ENTRIES];
static int fs_initialized = 0;
static int fat_dirty = 0;     // FAT нужно записать на диск
static int entries_dirty = 0; // Таблица записей нужна запись

/* Вычисление контрольной суммы FAT для проверки целостности */
static uint32_t calculate_fat_checksum(const fat16_table_t *table)
{
    uint32_t checksum = 0;
    const uint8_t *data = (const uint8_t *)table;
    size_t size = sizeof(fat16_table_t);

    for (size_t i = 0; i < size; i++)
    {
        checksum = ((checksum << 5) + checksum) + data[i]; // checksum * 33 + byte
    }

    return checksum;
}

/* Проверка целостности FAT */
static int verify_fat_integrity(const fat16_table_t *table)
{
    /* Проверка медиа-дескриптора */
    if (table->entries[0] != 0xFFF8)
        return 0;

    /* Проверка зарезервированного кластера */
    if (table->entries[1] != FAT_EOF)
        return 0;

    /* Проверка на явно некорректные значения */
    for (uint16_t i = 2; i < FAT_ENTRIES; i++)
    {
        uint16_t val = table->entries[i];
        /* Значение должно быть: 0 (свободно), FAT_EOF, или валидный номер кластера */
        if (val != FAT_FREE && val != FAT_EOF && (val < 2 || val >= FAT_ENTRIES))
            return 0;
    }

    return 1;
}

/* Чтение секторов с диска */
static int disk_read_sectors(uint64_t lba, uint32_t count, void *buffer)
{
    if (!g_disk || !fs_initialized)
        return FS_ERR_NOT_INITIALIZED;

    int rc = ide_read_sectors(g_disk, lba, count, buffer);
    if (rc != IDE_OK)
        return FS_ERR_DISK_IO;

    return FS_OK;
}

/* Запись секторов на диск */
static int disk_write_sectors(uint64_t lba, uint32_t count, const void *buffer)
{
    if (!g_disk || !fs_initialized)
        return FS_ERR_NOT_INITIALIZED;

    int rc = ide_write_sectors(g_disk, lba, count, buffer);
    if (rc != IDE_OK)
        return FS_ERR_DISK_IO;

    return FS_OK;
}

/* Загрузка FAT с диска с проверкой целостности и восстановлением */
static int load_fat(void)
{
    fat16_table_t fat1, fat2;
    int fat1_valid = 0;
    int fat2_valid = 0;
    int rc;

    /* Загрузка первой копии FAT */
    rc = disk_read_sectors(FAT1_START_LBA, SECTORS_PER_FAT, &fat1);
    if (rc == FS_OK)
    {
        fat1_valid = verify_fat_integrity(&fat1);
    }

    /* Загрузка второй копии FAT */
    rc = disk_read_sectors(FAT2_START_LBA, SECTORS_PER_FAT, &fat2);
    if (rc == FS_OK)
    {
        fat2_valid = verify_fat_integrity(&fat2);
    }

    /* Обе копии валидны - проверяем, совпадают ли они */
    if (fat1_valid && fat2_valid)
    {
        uint32_t checksum1 = calculate_fat_checksum(&fat1);
        uint32_t checksum2 = calculate_fat_checksum(&fat2);

        if (checksum1 == checksum2)
        {
            /* Обе копии идентичны - загружаем первую */
            memcpy(&fat, &fat1, sizeof(fat16_table_t));
            return FS_OK;
        }
        else
        {
            /* Копии отличаются - используем первую и восстанавливаем вторую */
            memcpy(&fat, &fat1, sizeof(fat16_table_t));

            /* Восстановление второй копии из первой */
            rc = disk_write_sectors(FAT2_START_LBA, SECTORS_PER_FAT, &fat1);
            if (rc != FS_OK)
            {
                /* Не удалось восстановить вторую копию, но продолжаем работу */
            }

            return FS_OK;
        }
    }

    /* Только первая копия валидна */
    if (fat1_valid && !fat2_valid)
    {
        memcpy(&fat, &fat1, sizeof(fat16_table_t));

        /* Восстановление второй копии из первой */
        rc = disk_write_sectors(FAT2_START_LBA, SECTORS_PER_FAT, &fat1);
        if (rc != FS_OK)
        {
            /* Не удалось восстановить, но продолжаем работу */
        }

        return FS_OK;
    }

    /* Только вторая копия валидна */
    if (!fat1_valid && fat2_valid)
    {
        memcpy(&fat, &fat2, sizeof(fat16_table_t));

        /* Восстановление первой копии из второй */
        rc = disk_write_sectors(FAT1_START_LBA, SECTORS_PER_FAT, &fat2);
        if (rc != FS_OK)
        {
            /* Не удалось восстановить, но продолжаем работу */
        }

        return FS_OK;
    }

    /* Обе копии повреждены или не читаются */
    return FS_ERR_DISK_IO;
}

/* Сохранение FAT на диск (обе копии) */
static int save_fat(void)
{
    int rc;

    /* Запись первой копии FAT */
    rc = disk_write_sectors(FAT1_START_LBA, SECTORS_PER_FAT, &fat);
    if (rc != FS_OK)
        return rc;

    /* Запись второй копии FAT */
    rc = disk_write_sectors(FAT2_START_LBA, SECTORS_PER_FAT, &fat);
    if (rc != FS_OK)
        return rc;

    fat_dirty = 0;
    return FS_OK;
}

/* Загрузка таблицы записей с диска */
static int load_entries(void)
{
    return disk_read_sectors(ENTRIES_TABLE_START_LBA, ENTRIES_TABLE_SECTORS, entries);
}

/* Сохранение таблицы записей на диск */
static int save_entries(void)
{
    int rc = disk_write_sectors(ENTRIES_TABLE_START_LBA, ENTRIES_TABLE_SECTORS, entries);
    if (rc == FS_OK)
        entries_dirty = 0;
    return rc;
}

/* Получить LBA начала кластера данных */
static uint64_t get_cluster_lba(uint16_t cluster)
{
    if (cluster < 2)
        return 0;
    return DATA_START_LBA + (uint64_t)(cluster - 2) * SECTORS_PER_CLUSTER;
}

/* Найти свободный кластер */
static uint16_t alloc_cluster(void)
{
    for (uint16_t i = 2; i < FAT_ENTRIES; i++)
    {
        if (fat.entries[i] == FAT_FREE)
        {
            fat.entries[i] = FAT_EOF;
            fat_dirty = 1;
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
        fat_dirty = 1;

        if (next == FAT_EOF)
            break;
        cur = next;
    }
}

/* Форматирование диска */
int fs_format(ide_disk_t *disk)
{
    if (!disk)
        return FS_ERR_INVALID_ARG;

    g_disk = disk;
    fs_initialized = 1;

    /* Создание загрузочного сектора */
    fs_boot_sector_t boot;
    memset(&boot, 0, sizeof(boot));
    boot.signature = FS_SIGNATURE;
    boot.total_sectors = (uint32_t)disk->total_sectors;
    boot.bytes_per_sector = BYTES_PER_SECTOR;
    boot.sectors_per_cluster = SECTORS_PER_CLUSTER;
    boot.reserved_sectors = RESERVED_SECTORS;
    boot.num_fats = NUM_FATS;

    int rc = disk_write_sectors(BOOT_SECTOR_LBA, 1, &boot);
    if (rc != FS_OK)
        return rc;

    /* Инициализация FAT */
    memset(&fat, 0, sizeof(fat));
    fat.entries[0] = 0xFFF8;  // Медиа-дескриптор
    fat.entries[1] = FAT_EOF; // Зарезервировано

    rc = save_fat();
    if (rc != FS_OK)
        return rc;

    /* Инициализация таблицы записей */
    memset(entries, 0, sizeof(entries));

    /* Создание корневого каталога */
    entries[FS_ROOT_IDX].used = 1;
    entries[FS_ROOT_IDX].is_dir = 1;
    entries[FS_ROOT_IDX].parent = -1;
    strncpy(entries[FS_ROOT_IDX].name, "/", FS_NAME_MAX - 1);
    entries[FS_ROOT_IDX].name[FS_NAME_MAX - 1] = '\0';
    entries[FS_ROOT_IDX].ext[0] = '\0';
    entries[FS_ROOT_IDX].first_cluster = 0;
    entries[FS_ROOT_IDX].size = 0;

    rc = save_entries();
    if (rc != FS_OK)
        return rc;

    fat_dirty = 0;
    entries_dirty = 0;

    return FS_OK;
}

/* Инициализация FS */
int fs_init(ide_disk_t *disk)
{
    if (!disk)
        return FS_ERR_INVALID_ARG;

    g_disk = disk;

    fs_initialized = 1;

    /* Чтение загрузочного сектора */
    fs_boot_sector_t boot;
    int rc = disk_read_sectors(BOOT_SECTOR_LBA, 1, &boot);
    if (rc != FS_OK)
        return rc;

    /* Проверка сигнатуры */
    if (boot.signature != FS_SIGNATURE)
    {
        /* Диск не отформатирован - нужно вызвать fs_format */
        fs_initialized = 0;
        return FS_ERR_NOT_FOUND;
    }

    /* Загрузка FAT */
    rc = load_fat();
    if (rc != FS_OK)
        return rc;

    /* Загрузка таблицы записей */
    rc = load_entries();
    if (rc != FS_OK)
        return rc;

    fat_dirty = 0;
    entries_dirty = 0;

    return FS_OK;
}

/* Синхронизация с диском */
int fs_sync(void)
{
    int rc = FS_OK;

    if (fat_dirty)
    {
        rc = save_fat();
        if (rc != FS_OK)
            return rc;
    }

    if (entries_dirty)
    {
        rc = save_entries();
        if (rc != FS_OK)
            return rc;
    }

    return FS_OK;
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
    if (!fs_initialized)
        return FS_ERR_NOT_INITIALIZED;

    if (!name || parent < 0 || parent >= FS_MAX_ENTRIES)
        return FS_ERR_INVALID_ARG;

    if (!entries[parent].used || !entries[parent].is_dir)
        return FS_ERR_NOT_DIR;

    // Проверим дубликат
    for (int i = 0; i < FS_MAX_ENTRIES; ++i)
    {
        if (entries[i].used && entries[i].parent == parent &&
            entries[i].is_dir && nameeq(entries[i].name, name, FS_NAME_MAX))
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

    entries_dirty = 1;

    return idx;
}

/* Удалить директорию (по индексу) - только если пуста */
int fs_rmdir(int dir_idx)
{
    if (!fs_initialized)
        return FS_ERR_NOT_INITIALIZED;

    if (dir_idx <= 0 || dir_idx >= FS_MAX_ENTRIES)
        return FS_ERR_INVALID_ARG;

    if (!entries[dir_idx].used || !entries[dir_idx].is_dir)
        return FS_ERR_NOT_DIR;

    if (has_children(dir_idx))
        return FS_ERR_EXISTS;

    entries[dir_idx].used = 0;
    entries_dirty = 1;

    return FS_OK;
}

/* Создать файл в каталоге parent */
int fs_create_file(const char *name, const char *ext, int parent, uint16_t *out_cluster)
{
    if (!fs_initialized)
        return FS_ERR_NOT_INITIALIZED;

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

    fat.entries[c] = FAT_EOF;
    fat_dirty = 1;
    entries_dirty = 1;

    if (out_cluster)
        *out_cluster = c;

    return idx;
}

/* Удалить запись (файл или пустую директорию) по индексу. Для файлов освобождает кластера */
int fs_remove_entry(int idx)
{
    if (!fs_initialized)
        return FS_ERR_NOT_INITIALIZED;

    if (idx <= 0 || idx >= FS_MAX_ENTRIES)
        return FS_ERR_INVALID_ARG;

    if (!entries[idx].used)
        return FS_ERR_NOT_DIR;

    if (entries[idx].is_dir)
    {
        if (has_children(idx))
            return FS_ERR_EXISTS;
        entries[idx].used = 0;
        entries_dirty = 1;
        return FS_OK;
    }
    else
    {
        uint16_t first = entries[idx].first_cluster;
        free_cluster_chain(first);
        entries[idx].used = 0;
        entries_dirty = 1;
        return FS_OK;
    }
}

/* Найти запись по имени/ext в каталоге parent */
int fs_find_in_dir(const char *name, const char *ext, int parent, fs_entry_t *out)
{
    if (!fs_initialized)
        return FS_ERR_NOT_INITIALIZED;

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
    if (!fs_initialized)
        return FS_ERR_NOT_INITIALIZED;

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
    if (!fs_initialized)
        return FS_ERR_NOT_INITIALIZED;

    if (first_cluster < 2 || first_cluster >= FAT_ENTRIES)
        return FS_ERR_INVALID_ARG;

    uint8_t *out = (uint8_t *)buf;
    uint16_t cur = first_cluster;
    size_t read = 0;

    /* Буфер для чтения кластера */
    uint8_t cluster_buf[BYTES_PER_CLUSTER];

    while (cur != 0 && cur < FAT_ENTRIES && read < size)
    {
        uint64_t lba = get_cluster_lba(cur);

        /* Чтение кластера с диска */
        int rc = disk_read_sectors(lba, SECTORS_PER_CLUSTER, cluster_buf);
        if (rc != FS_OK)
            return rc;

        size_t to_copy = size - read;
        if (to_copy > BYTES_PER_CLUSTER)
            to_copy = BYTES_PER_CLUSTER;

        memcpy(out + read, cluster_buf, to_copy);
        read += to_copy;

        if (fat.entries[cur] == FAT_EOF)
            break;
        cur = fat.entries[cur];
    }

    return read;
}

int fs_write(uint16_t first_cluster, const void *buf, size_t size)
{
    if (!fs_initialized)
        return FS_ERR_NOT_INITIALIZED;

    const uint8_t *data = (const uint8_t *)buf;

    if (first_cluster < 2 || first_cluster >= FAT_ENTRIES)
        return FS_ERR_INVALID_ARG;

    uint16_t cur = first_cluster;
    size_t written = 0;

    /* Буфер для записи кластера */
    uint8_t cluster_buf[BYTES_PER_CLUSTER];

    while (written < size)
    {
        uint64_t lba = get_cluster_lba(cur);

        size_t to_write = size - written;
        if (to_write > BYTES_PER_CLUSTER)
            to_write = BYTES_PER_CLUSTER;

        /* Копируем данные в буфер кластера */
        memcpy(cluster_buf, data + written, to_write);

        /* Если записываем неполный кластер, обнуляем остаток */
        if (to_write < BYTES_PER_CLUSTER)
            memset(cluster_buf + to_write, 0, BYTES_PER_CLUSTER - to_write);

        /* Запись кластера на диск */
        int rc = disk_write_sectors(lba, SECTORS_PER_CLUSTER, cluster_buf);
        if (rc != FS_OK)
            return rc;

        written += to_write;

        if (written < size)
        {
            if (fat.entries[cur] == FAT_EOF)
            {
                uint16_t nc = alloc_cluster();
                if (nc == FAT_NO_CLUSTER)
                {
                    fat.entries[cur] = FAT_EOF;
                    fat_dirty = 1;
                    return written;
                }
                fat.entries[cur] = nc;
                fat_dirty = 1;
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
            fat_dirty = 1;
            break;
        }
    }

    return written;
}

/* Высокоуровневые операции с файлами (по имени в каталоге) */
int fs_write_file_in_dir(const char *name, const char *ext, int parent, const void *data, size_t size)
{
    if (!fs_initialized)
        return FS_ERR_NOT_INITIALIZED;

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
        fat_dirty = 1;
        entries_dirty = 1;
    }

    if (size == 0)
    {
        entries[idx].size = 0;
        fat.entries[entries[idx].first_cluster] = FAT_EOF;
        fat_dirty = 1;
        entries_dirty = 1;
        return FS_OK;
    }

    int written = fs_write(entries[idx].first_cluster, data, size);
    if (written < 0)
        return written;

    if ((size_t)written != size)
    {
        entries[idx].size = (uint32_t)written;
        entries_dirty = 1;
        return FS_ERR_PARTIAL_WRITE;
    }

    entries[idx].size = (uint32_t)size;
    entries_dirty = 1;

    /* Синхронизация изменений с диском */
    return fs_sync();
}

int fs_read_file_in_dir(const char *name, const char *ext, int parent, void *buf, size_t bufsize, size_t *out_size)
{
    if (!fs_initialized)
        return FS_ERR_NOT_INITIALIZED;

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
    if (!fs_initialized)
        return FS_ERR_NOT_INITIALIZED;

    if (idx < 0 || idx >= FS_MAX_ENTRIES)
        return FS_ERR_INVALID_ARG;

    if (!entries[idx].used)
        return FS_ERR_INVALID_ARG;

    return entries[idx].parent;
}

int fs_build_path(int idx, char *buf, size_t size)
{
    if (!fs_initialized)
        return 0;

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