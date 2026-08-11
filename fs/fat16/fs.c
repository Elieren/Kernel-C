// fs.c
// Драйвер FAT16, совместимый со спецификацией Microsoft FAT (fatgen103) и драйверами vfat/msdos Linux; монтируется как обычный FAT16-диск.
// Layout тома: [BPB, 1 сектор][FAT #1][FAT #2][корневой каталог фикс. размера][область данных].
// Подкаталоги — обычные цепочки кластеров с 32-байтными записями, включая "." и "..".

#include "fs.h"
#include "lib/string/string.h"
#include "kernel/time/clock/clock.h"
#include <stdint.h>
#include <stddef.h>

/* ===================== Параметры и константы формата ===================== */

#define BYTES_PER_SECTOR 512u

/* Классическое значение: даёт 32 сектора (16384 байта) корневого каталога. */
#define ROOT_ENTRY_COUNT 512u

#define RESERVED_SECTORS 1u
#define NUM_FATS 2u

/* Макс. размер одной копии FAT в памяти: 256 секторов = 65536 записей — хватает на максимальный FAT16-том (65524 кластера + 2 служебные). */
#define FAT_MAX_SECTORS 256u
#define FAT_MAX_ENTRIES (FAT_MAX_SECTORS * BYTES_PER_SECTOR / 2u)

#define MEDIA_DESCRIPTOR 0xF8u /* жёсткий диск */
#define BOOT_SIG_EXT 0x29u     /* расширенная сигнатура BPB */
#define BOOT_SECTOR_SIGNATURE 0xAA55u

/* Диапазон числа кластеров для FAT16 по спецификации (меньше — FAT12, больше — FAT32). */
#define FAT16_MIN_CLUSTERS 4085u
#define FAT16_MAX_CLUSTERS 65524u

/* Специальные значения записи FAT. */
#define FAT_RESERVED_MIN 0xFFF0u
#define FAT_BAD_CLUSTER 0xFFF7u
#define FAT_EOC_MIN 0xFFF8u /* 0xFFF8..0xFFFF — конец цепочки кластеров */

/* Атрибуты 32-байтной записи каталога (DIR_Attr). */
#define ATTR_READ_ONLY 0x01u
#define ATTR_HIDDEN 0x02u
#define ATTR_SYSTEM 0x04u
#define ATTR_VOLUME_ID 0x08u
#define ATTR_DIRECTORY 0x10u
#define ATTR_ARCHIVE 0x20u
#define ATTR_LONG_NAME 0x0Fu

/* Биты DIR_NTRes: регистр короткого имени для показа "file.txt" вместо "FILE.TXT" без LFN. */
#define NTRES_NAME_LOWER 0x08u
#define NTRES_EXT_LOWER 0x10u

#define DIRENT_FREE 0x00u
#define DIRENT_DELETED 0xE5u

/* Короткие буферы под 8.3-имя/расширение (без учёта длинных имён). */
#define SFN_NAME_CAP 9 /* 8 символов + '\0' */
#define SFN_EXT_CAP 4  /* 3 символа + '\0' */

/* Ограничение на глубину пути в fs_build_path (защита стека). */
#define FS_MAX_PATH_DEPTH 128

/* ===================== On-disk структуры ===================== */

/* BIOS Parameter Block / загрузочный сектор FAT12/16 (ровно 512 байт). */
typedef struct __attribute__((packed))
{
    uint8_t jmp_boot[3];         // 0xEB,0x3C,0x90 — короткий переход, кода загрузки нет
    char oem_name[8];            // название "производителя"
    uint16_t bytes_per_sector;   // 512
    uint8_t sectors_per_cluster; // степень двойки: 1,2,4,...,64
    uint16_t reserved_sector_count;
    uint8_t num_fats;          // 2
    uint16_t root_entry_count; // ROOT_ENTRY_COUNT
    uint16_t total_sectors_16; // используется, если том <65536 секторов, иначе 0
    uint8_t media;             // 0xF8
    uint16_t fat_size_16;      // секторов на одну копию FAT
    uint16_t sectors_per_track;
    uint16_t num_heads;
    uint32_t hidden_sectors;
    uint32_t total_sectors_32; // используется, если total_sectors_16 == 0
    uint8_t drive_number;      // 0x80
    uint8_t reserved1;
    uint8_t boot_signature; // 0x29
    uint32_t volume_id;
    char volume_label[11];
    char fs_type[8]; // "FAT16   "
    uint8_t boot_code[448];
    uint16_t signature; // 0xAA55
} fat16_bpb_t;

_Static_assert(sizeof(fat16_bpb_t) == 512, "fat16_bpb_t must be exactly 512 bytes");

/* Стандартная 32-байтная запись каталога FAT (короткое имя 8.3). */
typedef struct __attribute__((packed))
{
    uint8_t name[11]; // 8.3, дополнено пробелами
    uint8_t attr;
    uint8_t nt_res;
    uint8_t crt_time_tenth;
    uint16_t crt_time;
    uint16_t crt_date;
    uint16_t lst_acc_date;
    uint16_t fst_clus_hi; // старшее слово кластера — всегда 0 в FAT12/16
    uint16_t wrt_time;
    uint16_t wrt_date;
    uint16_t fst_clus_lo; // младшее (и единственное значимое) слово кластера
    uint32_t file_size;
} fat_dirent_t;

_Static_assert(sizeof(fat_dirent_t) == 32, "fat_dirent_t must be exactly 32 bytes");

/* ===================== Внутрисеансовый кэш дескрипторов ===================== */
/* idx (fs.h) — лёгкий кэш-хэндл поверх диска: FAT16 не хранит inode-номеров, единственный устойчивый идентификатор записи — её положение (кластер каталога + слот); nodes[] лишь транслирует handle -> расположение. */
typedef struct
{
    uint8_t used;
    uint8_t is_dir;
    int16_t parent;              // idx родителя, -1 для корня
    uint16_t parent_dir_cluster; // 0 = запись лежит в корневом каталоге
    uint16_t dir_slot;           // номер 32-байтной записи внутри каталога родителя
    uint16_t start_cluster;      // первый кластер данных/содержимого; 0 = не выделен (для корня — всегда 0)
    uint32_t size;
} fs_node_t;

static fs_node_t nodes[FS_MAX_ENTRIES];

/* ===================== Глобальное состояние диска/тома ===================== */

static ide_disk_t *g_disk = NULL;
static int fs_initialized = 0;

static uint8_t g_sectors_per_cluster;
static uint16_t g_reserved_sectors;
static uint16_t g_root_entry_count;
static uint16_t g_fat_size_sectors;
static uint32_t g_total_sectors;

static uint32_t g_fat1_lba;
static uint32_t g_fat2_lba;
static uint32_t g_root_dir_lba;
static uint32_t g_root_dir_sectors;
static uint32_t g_data_lba;
static uint32_t g_count_of_clusters;
static uint32_t g_fat_entry_count; /* = g_count_of_clusters + 2 */
static uint32_t g_bytes_per_cluster;

static uint16_t fat_table[FAT_MAX_ENTRIES];
static uint16_t fat_table2[FAT_MAX_ENTRIES]; /* временный буфер, нужен только при монтировании */
static int fat_dirty = 0;

/* ===================== Низкоуровневый ввод-вывод ===================== */

static int disk_read_sectors(uint64_t lba, uint32_t count, void *buffer)
{
    if (!g_disk || !fs_initialized)
        return FS_ERR_NOT_INITIALIZED;

    int rc = ide_read_sectors(g_disk, lba, count, buffer);
    if (rc != IDE_OK)
        return FS_ERR_DISK_IO;

    return FS_OK;
}

static int disk_write_sectors(uint64_t lba, uint32_t count, const void *buffer)
{
    if (!g_disk || !fs_initialized)
        return FS_ERR_NOT_INITIALIZED;

    int rc = ide_write_sectors(g_disk, lba, count, buffer);
    if (rc != IDE_OK)
        return FS_ERR_DISK_IO;

    return FS_OK;
}

/* ===================== Геометрия тома ===================== */

static void apply_layout(const fat16_bpb_t *bpb)
{
    g_sectors_per_cluster = bpb->sectors_per_cluster;
    g_reserved_sectors = bpb->reserved_sector_count;
    g_root_entry_count = bpb->root_entry_count;
    g_fat_size_sectors = bpb->fat_size_16;
    g_total_sectors = bpb->total_sectors_16 ? bpb->total_sectors_16 : bpb->total_sectors_32;

    g_fat1_lba = g_reserved_sectors;
    g_fat2_lba = g_fat1_lba + g_fat_size_sectors;
    g_root_dir_sectors = ((uint32_t)g_root_entry_count * 32u + BYTES_PER_SECTOR - 1u) / BYTES_PER_SECTOR;
    g_root_dir_lba = g_fat2_lba + g_fat_size_sectors;
    g_data_lba = g_root_dir_lba + g_root_dir_sectors;

    if (g_total_sectors > g_data_lba && g_sectors_per_cluster > 0)
    {
        uint32_t data_sectors = g_total_sectors - g_data_lba;
        g_count_of_clusters = data_sectors / g_sectors_per_cluster;
    }
    else
    {
        g_count_of_clusters = 0;
    }

    g_fat_entry_count = g_count_of_clusters + 2u;
    g_bytes_per_cluster = (uint32_t)g_sectors_per_cluster * BYTES_PER_SECTOR;
}

/* Подбирает sectors_per_cluster и fat_size_16 так, чтобы число кластеров попало в диапазон FAT16, а FAT влезла в статический буфер; при избытке места используется только часть диска. */
static int choose_layout(uint32_t total_sectors, uint8_t *out_spc, uint16_t *out_fatsz, uint32_t *out_total_used)
{
    static const uint8_t spc_table[] = {1, 2, 4, 8, 16, 32, 64};
    const uint32_t root_dir_sectors = (ROOT_ENTRY_COUNT * 32u + BYTES_PER_SECTOR - 1u) / BYTES_PER_SECTOR;

    for (size_t i = 0; i < sizeof(spc_table); ++i)
    {
        uint8_t spc = spc_table[i];
        uint32_t avail = total_sectors;

        if (avail <= RESERVED_SECTORS + root_dir_sectors)
            continue;

        uint32_t tmp1 = avail - (RESERVED_SECTORS + root_dir_sectors);
        uint32_t tmp2 = 256u * spc + NUM_FATS;
        uint32_t fatsz = (tmp1 + tmp2 - 1u) / tmp2; /* формула из спецификации Microsoft FAT */

        if (fatsz == 0 || fatsz > FAT_MAX_SECTORS)
            continue;

        uint32_t data_sectors = avail - (RESERVED_SECTORS + NUM_FATS * fatsz + root_dir_sectors);
        uint32_t clusters = data_sectors / spc;

        if (clusters < FAT16_MIN_CLUSTERS)
            continue;

        if (clusters > FAT16_MAX_CLUSTERS)
        {
            /* диск больше максимального FAT16-тома при этом размере кластера — обрезаем */
            clusters = FAT16_MAX_CLUSTERS;
            data_sectors = clusters * (uint32_t)spc;
            avail = RESERVED_SECTORS + NUM_FATS * fatsz + root_dir_sectors + data_sectors;

            tmp1 = avail - (RESERVED_SECTORS + root_dir_sectors);
            fatsz = (tmp1 + tmp2 - 1u) / tmp2;
            if (fatsz == 0 || fatsz > FAT_MAX_SECTORS)
                continue;

            data_sectors = avail - (RESERVED_SECTORS + NUM_FATS * fatsz + root_dir_sectors);
            clusters = data_sectors / spc;
            if (clusters < FAT16_MIN_CLUSTERS || clusters > FAT16_MAX_CLUSTERS)
                continue;

            avail = RESERVED_SECTORS + NUM_FATS * fatsz + root_dir_sectors + clusters * (uint32_t)spc;
        }

        *out_spc = spc;
        *out_fatsz = (uint16_t)fatsz;
        *out_total_used = avail;
        return FS_OK;
    }

    return FS_ERR_DISK_TOO_SMALL;
}

static uint64_t cluster_to_lba(uint16_t cluster)
{
    if (cluster < 2)
        return 0;
    return (uint64_t)g_data_lba + (uint64_t)(cluster - 2) * g_sectors_per_cluster;
}

/* ===================== Работа с таблицей FAT ===================== */

static uint16_t alloc_cluster(void)
{
    for (uint32_t i = 2; i < g_fat_entry_count; ++i)
    {
        if (fat_table[i] == FAT_FREE)
        {
            fat_table[i] = FAT_EOF;
            fat_dirty = 1;
            return (uint16_t)i;
        }
    }
    return FAT_NO_CLUSTER;
}

static void free_cluster_chain(uint16_t first)
{
    if (first < 2 || first >= g_fat_entry_count)
        return;

    uint16_t cur = first;
    while (cur >= 2 && cur < g_fat_entry_count)
    {
        uint16_t next = fat_table[cur];
        fat_table[cur] = FAT_FREE;
        fat_dirty = 1;

        if (next == FAT_FREE || next >= FAT_EOC_MIN)
            break;
        cur = next;
    }
}

/* n-й (от 0) кластер в цепочке от start; 0, если цепочка короче. */
static uint16_t walk_chain(uint16_t start, uint32_t n)
{
    uint16_t cur = start;
    if (cur < 2 || cur >= g_fat_entry_count)
        return 0;

    for (uint32_t i = 0; i < n; ++i)
    {
        uint16_t nxt = fat_table[cur];
        if (nxt == FAT_FREE || nxt >= FAT_EOC_MIN)
            return 0;
        cur = nxt;
        if (cur < 2 || cur >= g_fat_entry_count)
            return 0;
    }
    return cur;
}

static int fat_copy_valid(const uint16_t *tbl)
{
    if (tbl[0] != (uint16_t)(0xFF00u | MEDIA_DESCRIPTOR))
        return 0;
    if (tbl[1] < FAT_EOC_MIN)
        return 0;

    for (uint32_t i = 2; i < g_fat_entry_count; ++i)
    {
        uint16_t v = tbl[i];
        if (v == FAT_FREE)
            continue;
        if (v >= 2 && v < g_fat_entry_count)
            continue;
        if (v >= FAT_RESERVED_MIN) /* бэд-кластер/резерв/конец цепочки */
            continue;
        return 0;
    }
    return 1;
}

static int load_fat(void)
{
    int rc1 = disk_read_sectors(g_fat1_lba, g_fat_size_sectors, fat_table);
    int rc2 = disk_read_sectors(g_fat2_lba, g_fat_size_sectors, fat_table2);

    int v1 = (rc1 == FS_OK) && fat_copy_valid(fat_table);
    int v2 = (rc2 == FS_OK) && fat_copy_valid(fat_table2);

    if (v1 && v2)
    {
        if (memcmp(fat_table, fat_table2, (size_t)g_fat_entry_count * 2u) != 0)
            disk_write_sectors(g_fat2_lba, g_fat_size_sectors, fat_table); /* рассинхрон — чиним вторую копию */
        return FS_OK;
    }
    if (v1)
    {
        disk_write_sectors(g_fat2_lba, g_fat_size_sectors, fat_table);
        return FS_OK;
    }
    if (v2)
    {
        memcpy(fat_table, fat_table2, (size_t)g_fat_entry_count * 2u);
        disk_write_sectors(g_fat1_lba, g_fat_size_sectors, fat_table);
        return FS_OK;
    }
    return FS_ERR_DISK_IO;
}

static int save_fat(void)
{
    int rc = disk_write_sectors(g_fat1_lba, g_fat_size_sectors, fat_table);
    if (rc != FS_OK)
        return rc;

    rc = disk_write_sectors(g_fat2_lba, g_fat_size_sectors, fat_table);
    if (rc != FS_OK)
        return rc;

    fat_dirty = 0;
    return FS_OK;
}

int fs_sync(void)
{
    if (!fs_initialized)
        return FS_ERR_NOT_INITIALIZED;
    if (fat_dirty)
        return save_fat();
    return FS_OK;
}

/* ===================== Даты/время каталожных записей ===================== */

static uint16_t fat_pack_date(int year, int month, int day)
{
    return (uint16_t)((((year - 1980) & 0x7F) << 9) | ((month & 0xF) << 5) | (day & 0x1F));
}

static uint16_t fat_pack_time(int hour, int min, int sec)
{
    return (uint16_t)(((hour & 0x1F) << 11) | ((min & 0x3F) << 5) | ((sec / 2) & 0x1F));
}

#define FS_DEFAULT_DATE fat_pack_date(system_date.year, system_date.month, system_date.day)
#define FS_DEFAULT_TIME fat_pack_time(system_clock.hh, system_clock.mm, system_clock.ss)

/* ===================== Короткие имена (8.3) ===================== */

static int is_valid_sfn_char(unsigned char c)
{
    if (c < 0x20 && c != 0x05)
        return 0;
    switch (c)
    {
    case '"':
    case '*':
    case '+':
    case ',':
    case '.':
    case '/':
    case ':':
    case ';':
    case '<':
    case '=':
    case '>':
    case '?':
    case '[':
    case '\\':
    case ']':
    case '|':
    case ' ':
        return 0;
    default:
        return 1;
    }
}

/* name/ext -> 11-байтное короткое имя FAT: обрезка до 8.3, верхний регистр, недопустимые символы -> '_'; выставляет NTRes для регистра в Linux. */
static void make_short_name(const char *name, const char *ext, uint8_t out11[11], uint8_t *nt_res_out)
{
    uint8_t nt = 0;
    int name_has_lower = 0, name_has_upper = 0;
    int ext_has_lower = 0, ext_has_upper = 0;

    for (int i = 0; i < 11; ++i)
        out11[i] = ' ';

    size_t nlen = name ? strlen(name) : 0;
    if (nlen > 8)
        nlen = 8;
    for (size_t i = 0; i < nlen; ++i)
    {
        unsigned char c = (unsigned char)name[i];
        if (c >= 'a' && c <= 'z')
        {
            name_has_lower = 1;
            c = (unsigned char)(c - 'a' + 'A');
        }
        else if (c >= 'A' && c <= 'Z')
        {
            name_has_upper = 1;
        }
        if (!is_valid_sfn_char(c))
            c = '_';
        out11[i] = c;
    }
    if (out11[0] == 0xE5) /* зарезервированное правило спецификации FAT */
        out11[0] = 0x05;

    size_t elen = ext ? strlen(ext) : 0;
    if (elen > 3)
        elen = 3;
    for (size_t i = 0; i < elen; ++i)
    {
        unsigned char c = (unsigned char)ext[i];
        if (c >= 'a' && c <= 'z')
        {
            ext_has_lower = 1;
            c = (unsigned char)(c - 'a' + 'A');
        }
        else if (c >= 'A' && c <= 'Z')
        {
            ext_has_upper = 1;
        }
        if (!is_valid_sfn_char(c))
            c = '_';
        out11[8 + i] = c;
    }

    if (name_has_lower && !name_has_upper)
        nt |= NTRES_NAME_LOWER;
    if (ext_has_lower && !ext_has_upper)
        nt |= NTRES_EXT_LOWER;

    if (nt_res_out)
        *nt_res_out = nt;
}

/* Обратное преобразование: короткое имя -> name/ext, пробелы справа обрезаются, регистр — по NTRes. */
static void parse_short_name(const uint8_t in11[11], uint8_t nt_res, char *name_out, size_t name_cap, char *ext_out, size_t ext_cap)
{
    uint8_t buf[11];
    memcpy(buf, in11, 11);
    if (buf[0] == 0x05)
        buf[0] = 0xE5;

    int nlen = 8;
    while (nlen > 0 && buf[nlen - 1] == ' ')
        nlen--;
    int elen = 3;
    while (elen > 0 && buf[8 + elen - 1] == ' ')
        elen--;

    int copy_n = nlen;
    if (name_cap > 0 && (size_t)copy_n >= name_cap)
        copy_n = (int)name_cap - 1;
    if (copy_n < 0)
        copy_n = 0;
    for (int i = 0; i < copy_n; ++i)
    {
        char c = (char)buf[i];
        if ((nt_res & NTRES_NAME_LOWER) && c >= 'A' && c <= 'Z')
            c = (char)(c - 'A' + 'a');
        name_out[i] = c;
    }
    if (name_cap > 0)
        name_out[copy_n] = '\0';

    int copy_e = elen;
    if (ext_cap > 0 && (size_t)copy_e >= ext_cap)
        copy_e = (int)ext_cap - 1;
    if (copy_e < 0)
        copy_e = 0;
    for (int i = 0; i < copy_e; ++i)
    {
        char c = (char)buf[8 + i];
        if ((nt_res & NTRES_EXT_LOWER) && c >= 'A' && c <= 'Z')
            c = (char)(c - 'A' + 'a');
        ext_out[i] = c;
    }
    if (ext_cap > 0)
        ext_out[copy_e] = '\0';
}

static int is_dot1_entry(const fat_dirent_t *de) { return de->name[0] == '.' && de->name[1] == ' '; }
static int is_dot2_entry(const fat_dirent_t *de) { return de->name[0] == '.' && de->name[1] == '.' && de->name[2] == ' '; }

/* ===================== Доступ к 32-байтным записям каталога ===================== */
/* start_cluster == 0 — корневой каталог (фиксированная область после FAT); иначе — цепочка кластеров в области данных. */

static int dirent_read(uint16_t start_cluster, uint32_t slot, fat_dirent_t *out)
{
    uint32_t lba, off;

    if (start_cluster == 0)
    {
        if (slot >= g_root_entry_count)
            return FS_ERR_INVALID_ARG;
        uint32_t byte_off = slot * 32u;
        lba = g_root_dir_lba + byte_off / BYTES_PER_SECTOR;
        off = byte_off % BYTES_PER_SECTOR;
    }
    else
    {
        uint32_t entries_per_cluster = g_bytes_per_cluster / 32u;
        uint32_t cl_index = slot / entries_per_cluster;
        uint32_t off_in_cl = slot % entries_per_cluster;

        uint16_t cl = walk_chain(start_cluster, cl_index);
        if (cl == 0)
            return FS_ERR_INVALID_ARG;

        uint32_t byte_off = off_in_cl * 32u;
        lba = (uint32_t)cluster_to_lba(cl) + byte_off / BYTES_PER_SECTOR;
        off = byte_off % BYTES_PER_SECTOR;
    }

    uint8_t buf[BYTES_PER_SECTOR];
    int rc = disk_read_sectors(lba, 1, buf);
    if (rc != FS_OK)
        return rc;

    memcpy(out, buf + off, sizeof(fat_dirent_t));
    return FS_OK;
}

static int dirent_write(uint16_t start_cluster, uint32_t slot, const fat_dirent_t *in)
{
    uint32_t lba, off;

    if (start_cluster == 0)
    {
        if (slot >= g_root_entry_count)
            return FS_ERR_INVALID_ARG;
        uint32_t byte_off = slot * 32u;
        lba = g_root_dir_lba + byte_off / BYTES_PER_SECTOR;
        off = byte_off % BYTES_PER_SECTOR;
    }
    else
    {
        uint32_t entries_per_cluster = g_bytes_per_cluster / 32u;
        uint32_t cl_index = slot / entries_per_cluster;
        uint32_t off_in_cl = slot % entries_per_cluster;

        uint16_t cl = walk_chain(start_cluster, cl_index);
        if (cl == 0)
            return FS_ERR_INVALID_ARG;

        uint32_t byte_off = off_in_cl * 32u;
        lba = (uint32_t)cluster_to_lba(cl) + byte_off / BYTES_PER_SECTOR;
        off = byte_off % BYTES_PER_SECTOR;
    }

    uint8_t buf[BYTES_PER_SECTOR];
    int rc = disk_read_sectors(lba, 1, buf);
    if (rc != FS_OK)
        return rc;

    memcpy(buf + off, in, sizeof(fat_dirent_t));
    return disk_write_sectors(lba, 1, buf);
}

/* Шаг перебора каталога, пропуская удалённые/LFN/метку тома; 1 = запись найдена, 0 = конец каталога, -1 = ошибка диска. */
static int dir_scan_step(uint16_t start_cluster, uint32_t *slot, fat_dirent_t *out)
{
    for (;;)
    {
        uint32_t s = *slot;
        int rc = dirent_read(start_cluster, s, out);
        if (rc == FS_ERR_INVALID_ARG)
            return 0; /* вышли за пределы выделенной каталогу области */
        if (rc != FS_OK)
            return -1;

        if (out->name[0] == DIRENT_FREE)
            return 0; /* официальный признак конца каталога */

        *slot = s + 1;

        if (out->name[0] == DIRENT_DELETED)
            continue;
        if (out->attr == ATTR_LONG_NAME)
            continue;
        if ((out->attr & ATTR_VOLUME_ID) && !(out->attr & ATTR_DIRECTORY))
            continue;

        return 1;
    }
}

static int dir_is_empty(uint16_t start_cluster)
{
    uint32_t slot = 0;
    fat_dirent_t de;
    while (dir_scan_step(start_cluster, &slot, &de) == 1)
    {
        if (start_cluster != 0 && (is_dot1_entry(&de) || is_dot2_entry(&de)))
            continue;
        return 0;
    }
    return 1;
}

/* Находит свободный слот (0x00/0xE5); для не-корневых каталогов при нехватке места растит цепочку новым кластером. */
static int dir_find_free_slot(uint16_t start_cluster, uint32_t *out_slot)
{
    uint32_t slot = 0;
    fat_dirent_t de;

    for (;;)
    {
        int rc = dirent_read(start_cluster, slot, &de);
        if (rc == FS_ERR_INVALID_ARG)
            break;
        if (rc != FS_OK)
            return rc;

        if (de.name[0] == DIRENT_FREE || de.name[0] == DIRENT_DELETED)
        {
            *out_slot = slot;
            return FS_OK;
        }
        slot++;
    }

    if (start_cluster == 0)
        return FS_ERR_NO_SPACE; /* корневой каталог фиксированного размера заполнен */

    uint16_t cur = start_cluster;
    for (;;)
    {
        if (cur < 2 || cur >= g_fat_entry_count)
            return FS_ERR_DISK_IO;
        uint16_t nxt = fat_table[cur];
        if (nxt == FAT_FREE || nxt >= FAT_EOC_MIN)
            break;
        cur = nxt;
    }

    uint16_t nc = alloc_cluster();
    if (nc == FAT_NO_CLUSTER)
        return FS_ERR_NO_FAT_SPACE;

    uint8_t zero_sector[BYTES_PER_SECTOR];
    memset(zero_sector, 0, sizeof(zero_sector));
    uint64_t base_lba = cluster_to_lba(nc);
    for (uint32_t s = 0; s < g_sectors_per_cluster; ++s)
    {
        int rc = disk_write_sectors(base_lba + s, 1, zero_sector);
        if (rc != FS_OK)
        {
            fat_table[nc] = FAT_FREE;
            return rc;
        }
    }

    fat_table[cur] = nc;
    fat_table[nc] = FAT_EOF;
    fat_dirty = 1;

    *out_slot = slot; /* ровно первый индекс только что добавленного кластера */
    return FS_OK;
}

/* ===================== Кэш дескрипторов (idx) ===================== */

static int find_or_alloc_node(int16_t parent, uint16_t parent_dir_cluster, uint16_t dir_slot,
                              uint8_t is_dir, uint16_t start_cluster, uint32_t size)
{
    for (int i = ENTRY_START_IDX; i < FS_MAX_ENTRIES; ++i)
    {
        if (nodes[i].used && nodes[i].parent == parent &&
            nodes[i].parent_dir_cluster == parent_dir_cluster && nodes[i].dir_slot == dir_slot)
        {
            nodes[i].is_dir = is_dir;
            nodes[i].start_cluster = start_cluster;
            nodes[i].size = size;
            return i;
        }
    }
    for (int i = ENTRY_START_IDX; i < FS_MAX_ENTRIES; ++i)
    {
        if (!nodes[i].used)
        {
            nodes[i].used = 1;
            nodes[i].is_dir = is_dir;
            nodes[i].parent = parent;
            nodes[i].parent_dir_cluster = parent_dir_cluster;
            nodes[i].dir_slot = dir_slot;
            nodes[i].start_cluster = start_cluster;
            nodes[i].size = size;
            return i;
        }
    }
    return FS_ERR_NO_SPACE;
}

/* ===================== Форматирование и монтирование ===================== */

int fs_format(ide_disk_t *disk)
{
    if (!disk)
        return FS_ERR_INVALID_ARG;

    g_disk = disk;
    fs_initialized = 1;

    uint32_t phys_sectors = (disk->total_sectors > 0xFFFFFFFFull) ? 0xFFFFFFFFu : (uint32_t)disk->total_sectors;

    uint8_t spc;
    uint16_t fatsz;
    uint32_t total_used;
    int rc = choose_layout(phys_sectors, &spc, &fatsz, &total_used);
    if (rc != FS_OK)
    {
        fs_initialized = 0;
        return rc;
    }

    fat16_bpb_t bpb;
    memset(&bpb, 0, sizeof(bpb));
    bpb.jmp_boot[0] = 0xEB;
    bpb.jmp_boot[1] = 0x3C;
    bpb.jmp_boot[2] = 0x90;
    memcpy(bpb.oem_name, "KERNELC ", 8);
    bpb.bytes_per_sector = BYTES_PER_SECTOR;
    bpb.sectors_per_cluster = spc;
    bpb.reserved_sector_count = RESERVED_SECTORS;
    bpb.num_fats = NUM_FATS;
    bpb.root_entry_count = ROOT_ENTRY_COUNT;
    if (total_used < 0x10000u)
    {
        bpb.total_sectors_16 = (uint16_t)total_used;
        bpb.total_sectors_32 = 0;
    }
    else
    {
        bpb.total_sectors_16 = 0;
        bpb.total_sectors_32 = total_used;
    }
    bpb.media = MEDIA_DESCRIPTOR;
    bpb.fat_size_16 = fatsz;
    bpb.sectors_per_track = 63;
    bpb.num_heads = 255;
    bpb.hidden_sectors = 0;
    bpb.drive_number = 0x80;
    bpb.reserved1 = 0;
    bpb.boot_signature = BOOT_SIG_EXT;
    bpb.volume_id = 0x4B43313Au; /* произвольный, но фиксированный идентификатор тома */
    memset(bpb.volume_label, ' ', sizeof(bpb.volume_label));
    memcpy(bpb.volume_label, "KERNEL-C", 8);
    memcpy(bpb.fs_type, "FAT16   ", 8);
    bpb.signature = BOOT_SECTOR_SIGNATURE;

    rc = disk_write_sectors(0, 1, &bpb);
    if (rc != FS_OK)
    {
        fs_initialized = 0;
        return rc;
    }

    apply_layout(&bpb);

    /* --- таблица FAT --- */
    memset(fat_table, 0, sizeof(fat_table));
    fat_table[0] = (uint16_t)(0xFF00u | MEDIA_DESCRIPTOR);
    fat_table[1] = FAT_EOF;
    rc = save_fat();
    if (rc != FS_OK)
    {
        fs_initialized = 0;
        return rc;
    }

    /* --- корневой каталог: обнуляем всю область --- */
    uint8_t zero_sector[BYTES_PER_SECTOR];
    memset(zero_sector, 0, sizeof(zero_sector));
    for (uint32_t s = 0; s < g_root_dir_sectors; ++s)
    {
        rc = disk_write_sectors(g_root_dir_lba + s, 1, zero_sector);
        if (rc != FS_OK)
        {
            fs_initialized = 0;
            return rc;
        }
    }

    /* метка тома первой записью корня — необязательно, но помогает опознать диск (lsblk/blkid) */
    fat_dirent_t label;
    memset(&label, 0, sizeof(label));
    memset(label.name, ' ', sizeof(label.name));
    memcpy(label.name, "KERNEL-C", 8);
    label.attr = ATTR_VOLUME_ID;
    label.crt_date = label.wrt_date = FS_DEFAULT_DATE;
    label.crt_time = label.wrt_time = FS_DEFAULT_TIME;
    rc = dirent_write(0, 0, &label);
    if (rc != FS_OK)
    {
        fs_initialized = 0;
        return rc;
    }

    memset(nodes, 0, sizeof(nodes));
    nodes[FS_ROOT_IDX].used = 1;
    nodes[FS_ROOT_IDX].is_dir = 1;
    nodes[FS_ROOT_IDX].parent = -1;
    nodes[FS_ROOT_IDX].start_cluster = 0;

    fat_dirty = 0;
    return FS_OK;
}

int fs_init(ide_disk_t *disk)
{
    if (!disk)
        return FS_ERR_INVALID_ARG;

    g_disk = disk;
    fs_initialized = 1;

    fat16_bpb_t bpb;
    int rc = disk_read_sectors(0, 1, &bpb);
    if (rc != FS_OK)
    {
        fs_initialized = 0;
        return rc;
    }

    if (bpb.signature != BOOT_SECTOR_SIGNATURE ||
        bpb.bytes_per_sector != BYTES_PER_SECTOR ||
        bpb.num_fats == 0 ||
        bpb.sectors_per_cluster == 0 ||
        memcmp(bpb.fs_type, "FAT16   ", 8) != 0)
    {
        fs_initialized = 0;
        return FS_ERR_NOT_FOUND;
    }

    apply_layout(&bpb);

    if (g_fat_size_sectors == 0 || g_fat_size_sectors > FAT_MAX_SECTORS ||
        g_count_of_clusters < FAT16_MIN_CLUSTERS || g_count_of_clusters > FAT16_MAX_CLUSTERS)
    {
        fs_initialized = 0;
        return FS_ERR_NOT_FOUND;
    }

    rc = load_fat();
    if (rc != FS_OK)
    {
        fs_initialized = 0;
        return rc;
    }

    memset(nodes, 0, sizeof(nodes));
    nodes[FS_ROOT_IDX].used = 1;
    nodes[FS_ROOT_IDX].is_dir = 1;
    nodes[FS_ROOT_IDX].parent = -1;
    nodes[FS_ROOT_IDX].start_cluster = 0;

    fat_dirty = 0;
    return FS_OK;
}

/* ===================== Каталоги ===================== */

int fs_mkdir(const char *name, int parent)
{
    if (!fs_initialized)
        return FS_ERR_NOT_INITIALIZED;
    if (!name || !name[0] || parent < 0 || parent >= FS_MAX_ENTRIES)
        return FS_ERR_INVALID_ARG;
    if (!nodes[parent].used || !nodes[parent].is_dir)
        return FS_ERR_NOT_DIR;

    uint16_t pdc = nodes[parent].start_cluster;

    if (fs_find_in_dir(name, "", parent, NULL) >= 0)
        return FS_ERR_EXISTS;

    uint8_t target[11];
    uint8_t nt_res;
    make_short_name(name, "", target, &nt_res);

    uint32_t free_slot;
    int rc = dir_find_free_slot(pdc, &free_slot);
    if (rc != FS_OK)
        return rc;

    uint16_t nc = alloc_cluster();
    if (nc == FAT_NO_CLUSTER)
        return FS_ERR_NO_FAT_SPACE;

    uint8_t zero_sector[BYTES_PER_SECTOR];
    memset(zero_sector, 0, sizeof(zero_sector));
    uint64_t base_lba = cluster_to_lba(nc);
    for (uint32_t s = 0; s < g_sectors_per_cluster; ++s)
    {
        rc = disk_write_sectors(base_lba + s, 1, zero_sector);
        if (rc != FS_OK)
        {
            fat_table[nc] = FAT_FREE;
            return rc;
        }
    }
    fat_table[nc] = FAT_EOF;
    fat_dirty = 1;

    fat_dirent_t dot;
    memset(&dot, 0, sizeof(dot));
    memset(dot.name, ' ', sizeof(dot.name));
    dot.name[0] = '.';
    dot.attr = ATTR_DIRECTORY;
    dot.fst_clus_lo = nc;
    dot.crt_date = dot.wrt_date = FS_DEFAULT_DATE;
    dot.crt_time = dot.wrt_time = FS_DEFAULT_TIME;
    rc = dirent_write(nc, 0, &dot);
    if (rc != FS_OK)
        return rc;

    fat_dirent_t dotdot;
    memset(&dotdot, 0, sizeof(dotdot));
    memset(dotdot.name, ' ', sizeof(dotdot.name));
    dotdot.name[0] = '.';
    dotdot.name[1] = '.';
    dotdot.attr = ATTR_DIRECTORY;
    dotdot.fst_clus_lo = pdc; /* для родителя-корня так и остаётся 0 — требование спецификации */
    dotdot.crt_date = dotdot.wrt_date = FS_DEFAULT_DATE;
    dotdot.crt_time = dotdot.wrt_time = FS_DEFAULT_TIME;
    rc = dirent_write(nc, 1, &dotdot);
    if (rc != FS_OK)
        return rc;

    fat_dirent_t entry;
    memset(&entry, 0, sizeof(entry));
    memcpy(entry.name, target, 11);
    entry.attr = ATTR_DIRECTORY;
    entry.nt_res = nt_res;
    entry.fst_clus_lo = nc;
    entry.crt_date = entry.wrt_date = FS_DEFAULT_DATE;
    entry.crt_time = entry.wrt_time = FS_DEFAULT_TIME;
    rc = dirent_write(pdc, free_slot, &entry);
    if (rc != FS_OK)
        return rc;

    int idx = find_or_alloc_node((int16_t)parent, pdc, (uint16_t)free_slot, 1, nc, 0);
    if (idx < 0)
        return idx;

    fs_sync();
    return idx;
}

int fs_rmdir(int dir_idx)
{
    if (!fs_initialized)
        return FS_ERR_NOT_INITIALIZED;
    if (dir_idx <= 0 || dir_idx >= FS_MAX_ENTRIES)
        return FS_ERR_INVALID_ARG;
    if (!nodes[dir_idx].used || !nodes[dir_idx].is_dir)
        return FS_ERR_NOT_DIR;

    uint16_t sc = nodes[dir_idx].start_cluster;
    if (!dir_is_empty(sc))
        return FS_ERR_EXISTS;

    fat_dirent_t de;
    int rc = dirent_read(nodes[dir_idx].parent_dir_cluster, nodes[dir_idx].dir_slot, &de);
    if (rc != FS_OK)
        return rc;

    de.name[0] = DIRENT_DELETED;
    rc = dirent_write(nodes[dir_idx].parent_dir_cluster, nodes[dir_idx].dir_slot, &de);
    if (rc != FS_OK)
        return rc;

    free_cluster_chain(sc);
    nodes[dir_idx].used = 0;

    fs_sync();
    return FS_OK;
}

/* ===================== Файлы ===================== */

int fs_create_file(const char *name, const char *ext, int parent, uint16_t *out_cluster)
{
    if (!fs_initialized)
        return FS_ERR_NOT_INITIALIZED;
    if (!name || !name[0] || parent < 0 || parent >= FS_MAX_ENTRIES)
        return FS_ERR_INVALID_ARG;
    if (!nodes[parent].used || !nodes[parent].is_dir)
        return FS_ERR_NOT_DIR;

    const char *ext_cmp = ext ? ext : "";
    uint16_t pdc = nodes[parent].start_cluster;

    if (fs_find_in_dir(name, ext_cmp, parent, NULL) >= 0)
        return FS_ERR_EXISTS;

    uint8_t target[11];
    uint8_t nt_res;
    make_short_name(name, ext_cmp, target, &nt_res);

    uint32_t free_slot;
    int rc = dir_find_free_slot(pdc, &free_slot);
    if (rc != FS_OK)
        return rc;

    uint16_t c = alloc_cluster();
    if (c == FAT_NO_CLUSTER)
        return FS_ERR_NO_FAT_SPACE;

    fat_dirent_t entry;
    memset(&entry, 0, sizeof(entry));
    memcpy(entry.name, target, 11);
    entry.attr = ATTR_ARCHIVE;
    entry.nt_res = nt_res;
    entry.fst_clus_lo = c;
    entry.file_size = 0;
    entry.crt_date = entry.wrt_date = FS_DEFAULT_DATE;
    entry.crt_time = entry.wrt_time = FS_DEFAULT_TIME;
    rc = dirent_write(pdc, free_slot, &entry);
    if (rc != FS_OK)
    {
        fat_table[c] = FAT_FREE;
        return rc;
    }

    int idx = find_or_alloc_node((int16_t)parent, pdc, (uint16_t)free_slot, 0, c, 0);
    if (idx < 0)
        return idx;

    if (out_cluster)
        *out_cluster = c;

    fs_sync();
    return idx;
}

int fs_remove_entry(int idx)
{
    if (!fs_initialized)
        return FS_ERR_NOT_INITIALIZED;
    if (idx <= 0 || idx >= FS_MAX_ENTRIES)
        return FS_ERR_INVALID_ARG;
    if (!nodes[idx].used)
        return FS_ERR_NOT_DIR;

    if (nodes[idx].is_dir && !dir_is_empty(nodes[idx].start_cluster))
        return FS_ERR_EXISTS;

    fat_dirent_t de;
    int rc = dirent_read(nodes[idx].parent_dir_cluster, nodes[idx].dir_slot, &de);
    if (rc != FS_OK)
        return rc;

    de.name[0] = DIRENT_DELETED;
    rc = dirent_write(nodes[idx].parent_dir_cluster, nodes[idx].dir_slot, &de);
    if (rc != FS_OK)
        return rc;

    free_cluster_chain(nodes[idx].start_cluster);
    nodes[idx].used = 0;

    fs_sync();
    return FS_OK;
}

int fs_find_in_dir(const char *name, const char *ext, int parent, fs_entry_t *out)
{
    if (!fs_initialized)
        return FS_ERR_NOT_INITIALIZED;
    if (!name || parent < 0 || parent >= FS_MAX_ENTRIES)
        return FS_ERR_INVALID_ARG;
    if (!nodes[parent].used || !nodes[parent].is_dir)
        return FS_ERR_NOT_DIR;

    uint16_t pdc = nodes[parent].start_cluster;

    /* "." и ".." — обычные записи каталога (fs_mkdir кладёт их первыми двумя слотами), поэтому разрешаются здесь же, как любое другое имя. */
    int is_dot1 = name[0] == '.' && name[1] == '\0' && (!ext || !ext[0]);
    int is_dot2 = name[0] == '.' && name[1] == '.' && name[2] == '\0' && (!ext || !ext[0]);

    if (is_dot1 || is_dot2)
    {
        int idx;

        if (is_dot1)
        {
            /* "." — тот же каталог, в котором искали (FstClus записи "." всегда равен pdc) */
            idx = parent;
        }
        else if (pdc == 0)
        {
            /* у корневого каталога нет записи ".." — это фиксированная область, а не подкаталог; выше корня некуда */
            idx = FS_ROOT_IDX;
        }
        else
        {
            fat_dirent_t dotdot;
            if (dirent_read(pdc, 1, &dotdot) != FS_OK || !is_dot2_entry(&dotdot))
                return FS_ERR_NOT_FOUND; /* каталог повреждён */

            uint16_t target_cluster = dotdot.fst_clus_lo;
            int16_t cached_parent = nodes[parent].parent;

            if (target_cluster == 0)
            {
                idx = FS_ROOT_IDX;
            }
            else if (cached_parent >= 0 && nodes[cached_parent].used &&
                     nodes[cached_parent].start_cluster == target_cluster)
            {
                /* быстрый путь: кэш подтверждён содержимым записи ".." на диске */
                idx = cached_parent;
            }
            else
            {
                /* кэш разошёлся с диском — ищем среди известных узлов тот, что указывает на нужный кластер */
                idx = -1;
                for (int i = FS_ROOT_IDX; i < FS_MAX_ENTRIES; ++i)
                {
                    if (nodes[i].used && nodes[i].is_dir && nodes[i].start_cluster == target_cluster)
                    {
                        idx = i;
                        break;
                    }
                }
                if (idx < 0)
                    return FS_ERR_NOT_FOUND;
            }
        }

        if (out)
        {
            memset(out, 0, sizeof(*out));
            out->name[0] = '.';
            if (is_dot2)
                out->name[1] = '.';
            out->parent = (int16_t)parent;
            out->first_cluster = nodes[idx].start_cluster;
            out->size = 0;
            out->used = 1;
            out->is_dir = 1;
        }
        return idx;
    }

    uint8_t target[11];
    uint8_t nt_res;
    make_short_name(name, ext ? ext : "", target, &nt_res);

    uint32_t slot = 0;
    fat_dirent_t de;
    while (dir_scan_step(pdc, &slot, &de) == 1)
    {
        uint32_t this_slot = slot - 1;

        if (pdc != 0 && (is_dot1_entry(&de) || is_dot2_entry(&de)))
            continue;
        if (memcmp(de.name, target, 11) != 0)
            continue;

        int is_dir_entry = (de.attr & ATTR_DIRECTORY) != 0;
        uint16_t sc = de.fst_clus_lo;
        uint32_t sz = de.file_size;

        int idx = find_or_alloc_node((int16_t)parent, pdc, (uint16_t)this_slot, is_dir_entry, sc, sz);
        if (idx < 0)
            return idx;

        if (out)
        {
            memset(out, 0, sizeof(*out));
            parse_short_name(de.name, de.nt_res, out->name, FS_NAME_MAX, out->ext, FS_EXT_MAX);
            out->parent = (int16_t)parent;
            out->first_cluster = sc;
            out->size = sz;
            out->used = 1;
            out->is_dir = (uint8_t)is_dir_entry;
        }
        return idx;
    }

    return FS_ERR_NOT_FOUND;
}

int fs_get_all_in_dir(fs_entry_t *out_files, int max_files, int parent)
{
    if (!fs_initialized)
        return FS_ERR_NOT_INITIALIZED;
    if (parent < 0 || parent >= FS_MAX_ENTRIES)
        return FS_ERR_INVALID_ARG;
    if (!nodes[parent].used || !nodes[parent].is_dir)
        return FS_ERR_NOT_DIR;

    uint16_t pdc = nodes[parent].start_cluster;
    int count = 0;
    uint32_t slot = 0;
    fat_dirent_t de;

    while (count < max_files && dir_scan_step(pdc, &slot, &de) == 1)
    {
        uint32_t this_slot = slot - 1;

        if (pdc != 0 && (is_dot1_entry(&de) || is_dot2_entry(&de)))
            continue;

        int is_dir_entry = (de.attr & ATTR_DIRECTORY) != 0;
        uint16_t sc = de.fst_clus_lo;
        uint32_t sz = de.file_size;

        find_or_alloc_node((int16_t)parent, pdc, (uint16_t)this_slot, is_dir_entry, sc, sz);

        fs_entry_t *o = &out_files[count];
        memset(o, 0, sizeof(*o));

        char nbuf[FS_NAME_MAX];
        char ebuf[FS_EXT_MAX];
        parse_short_name(de.name, de.nt_res, nbuf, sizeof(nbuf), ebuf, sizeof(ebuf));

        size_t nlen = strlen(nbuf);
        if (is_dir_entry && nlen < FS_NAME_MAX - 2)
        {
            memcpy(o->name, nbuf, nlen);
            o->name[nlen] = '/';
            o->name[nlen + 1] = '\0';
        }
        else
        {
            strncpy(o->name, nbuf, FS_NAME_MAX - 1);
            o->name[FS_NAME_MAX - 1] = '\0';
        }
        strncpy(o->ext, ebuf, FS_EXT_MAX - 1);
        o->ext[FS_EXT_MAX - 1] = '\0';

        o->parent = (int16_t)parent;
        o->first_cluster = sc;
        o->size = sz;
        o->used = 1;
        o->is_dir = (uint8_t)is_dir_entry;

        count++;
    }

    return count;
}

/* ===================== Низкоуровневые чтение/запись данных файла ===================== */

int fs_read(uint16_t first_cluster, void *buf, size_t size)
{
    if (!fs_initialized)
        return FS_ERR_NOT_INITIALIZED;
    if (first_cluster < 2 || first_cluster >= g_fat_entry_count)
        return FS_ERR_INVALID_ARG;

    uint8_t *out = (uint8_t *)buf;
    uint16_t cur = first_cluster;
    size_t done = 0;
    uint8_t sector_buf[BYTES_PER_SECTOR];

    while (cur >= 2 && cur < g_fat_entry_count && done < size)
    {
        uint64_t base_lba = cluster_to_lba(cur);
        for (uint32_t s = 0; s < g_sectors_per_cluster && done < size; ++s)
        {
            int rc = disk_read_sectors(base_lba + s, 1, sector_buf);
            if (rc != FS_OK)
                return rc;

            size_t to_copy = size - done;
            if (to_copy > BYTES_PER_SECTOR)
                to_copy = BYTES_PER_SECTOR;

            memcpy(out + done, sector_buf, to_copy);
            done += to_copy;
        }

        uint16_t nxt = fat_table[cur];
        if (nxt == FAT_FREE || nxt >= FAT_EOC_MIN)
            break;
        cur = nxt;
    }

    return (int)done;
}

int fs_write(uint16_t first_cluster, const void *buf, size_t size)
{
    if (!fs_initialized)
        return FS_ERR_NOT_INITIALIZED;
    if (first_cluster < 2 || first_cluster >= g_fat_entry_count)
        return FS_ERR_INVALID_ARG;

    const uint8_t *data = (const uint8_t *)buf;
    uint16_t cur = first_cluster;
    size_t written = 0;
    uint8_t sector_buf[BYTES_PER_SECTOR];

    while (written < size)
    {
        uint64_t base_lba = cluster_to_lba(cur);

        for (uint32_t s = 0; s < g_sectors_per_cluster && written < size; ++s)
        {
            size_t to_write = size - written;
            if (to_write >= BYTES_PER_SECTOR)
            {
                memcpy(sector_buf, data + written, BYTES_PER_SECTOR);
                to_write = BYTES_PER_SECTOR;
            }
            else
            {
                memcpy(sector_buf, data + written, to_write);
                memset(sector_buf + to_write, 0, BYTES_PER_SECTOR - to_write);
            }

            int rc = disk_write_sectors(base_lba + s, 1, sector_buf);
            if (rc != FS_OK)
                return rc;

            written += to_write;
        }

        if (written < size)
        {
            uint16_t nxt = fat_table[cur];
            if (nxt == FAT_FREE || nxt >= FAT_EOC_MIN)
            {
                uint16_t nc = alloc_cluster();
                if (nc == FAT_NO_CLUSTER)
                {
                    fat_table[cur] = FAT_EOF;
                    fat_dirty = 1;
                    return (int)written;
                }
                fat_table[cur] = nc;
                fat_dirty = 1;
                cur = nc;
            }
            else
            {
                cur = nxt;
            }
        }
        else
        {
            fat_table[cur] = FAT_EOF;
            fat_dirty = 1;
        }
    }

    return (int)written;
}

/* ===================== Высокоуровневые операции с файлами ===================== */

int fs_write_file_in_dir(const char *name, const char *ext, int parent, const void *data, size_t size)
{
    if (!fs_initialized)
        return FS_ERR_NOT_INITIALIZED;
    if (!name)
        return FS_ERR_INVALID_ARG;
    if (parent < 0 || parent >= FS_MAX_ENTRIES)
        return FS_ERR_INVALID_ARG;
    if (!nodes[parent].used || !nodes[parent].is_dir)
        return FS_ERR_NOT_DIR;

    int idx = fs_find_in_dir(name, ext, parent, NULL);
    uint16_t cluster;

    if (idx < 0)
    {
        int cidx = fs_create_file(name, ext, parent, &cluster);
        if (cidx < 0)
            return cidx;
        idx = cidx;
    }
    else
    {
        uint16_t old = nodes[idx].start_cluster;

        cluster = alloc_cluster();
        if (cluster == FAT_NO_CLUSTER)
            return FS_ERR_NO_FAT_SPACE; /* места нет, старые данные не тронуты */

        if (old)
            free_cluster_chain(old);

        nodes[idx].start_cluster = cluster;
        fat_table[cluster] = FAT_EOF;
        fat_dirty = 1;
    }

    int written = 0;
    if (size > 0)
    {
        written = fs_write(cluster, data, size);
        if (written < 0)
            return written;
    }
    else
    {
        fat_table[cluster] = FAT_EOF;
        fat_dirty = 1;
    }

    nodes[idx].size = (uint32_t)written;

    fat_dirent_t de;
    int rc = dirent_read(nodes[idx].parent_dir_cluster, nodes[idx].dir_slot, &de);
    if (rc != FS_OK)
        return rc;

    de.fst_clus_lo = cluster;
    de.file_size = (uint32_t)written;
    de.wrt_date = FS_DEFAULT_DATE;
    de.wrt_time = FS_DEFAULT_TIME;

    rc = dirent_write(nodes[idx].parent_dir_cluster, nodes[idx].dir_slot, &de);
    if (rc != FS_OK)
        return rc;

    int sync_rc = fs_sync();
    if (sync_rc != FS_OK)
        return sync_rc;

    if ((size_t)written != size)
        return FS_ERR_PARTIAL_WRITE;

    return FS_OK;
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

    if (f.size == 0)
    {
        if (out_size)
            *out_size = 0;
        return FS_OK;
    }

    int r = fs_read(f.first_cluster, buf, f.size);
    if (r < 0)
        return r;

    if (out_size)
        *out_size = (size_t)r;

    return FS_OK;
}

/* ===================== Путь и родитель ===================== */

int fs_get_parent_idx(int idx)
{
    if (!fs_initialized)
        return FS_ERR_NOT_INITIALIZED;
    if (idx < 0 || idx >= FS_MAX_ENTRIES)
        return FS_ERR_INVALID_ARG;
    if (!nodes[idx].used)
        return FS_ERR_INVALID_ARG;

    return nodes[idx].parent;
}

int fs_build_path(int idx, char *buf, size_t size)
{
    if (!fs_initialized)
        return 0;
    if (!buf || size == 0)
        return 0;
    if (idx < 0 || idx >= FS_MAX_ENTRIES)
        return 0;
    if (!nodes[idx].used)
        return 0;

    if (idx == FS_ROOT_IDX)
    {
        if (size < 2)
            return 0;
        buf[0] = '/';
        buf[1] = '\0';
        return 1;
    }

    /* Компоненты пути собираются от элемента к корню; имя каждого предка перечитывается из его записи каталога (кэш имён не хранит). */
    static char comp[FS_MAX_PATH_DEPTH][SFN_NAME_CAP + 1 + SFN_EXT_CAP];
    int depth = 0;
    int cur = idx;

    while (cur != FS_ROOT_IDX && cur >= 0 && cur < FS_MAX_ENTRIES && nodes[cur].used && depth < FS_MAX_PATH_DEPTH)
    {
        fat_dirent_t de;
        if (dirent_read(nodes[cur].parent_dir_cluster, nodes[cur].dir_slot, &de) != FS_OK)
            return 0;

        char nbuf[SFN_NAME_CAP];
        char ebuf[SFN_EXT_CAP];
        parse_short_name(de.name, de.nt_res, nbuf, sizeof(nbuf), ebuf, sizeof(ebuf));

        char *dst = comp[depth];
        size_t pos = 0;
        for (; nbuf[pos] && pos < sizeof(nbuf) - 1; ++pos)
            dst[pos] = nbuf[pos];
        if (!(de.attr & ATTR_DIRECTORY) && ebuf[0])
        {
            dst[pos++] = '.';
            for (size_t j = 0; ebuf[j] && j < sizeof(ebuf) - 1; ++j)
                dst[pos++] = ebuf[j];
        }
        dst[pos] = '\0';

        depth++;
        cur = nodes[cur].parent;
    }

    if (cur != FS_ROOT_IDX)
        return 0; /* повреждённая цепочка родителей либо путь слишком глубок */

    size_t total = 1;
    for (int i = 0; i < depth; ++i)
        total += 1 + strlen(comp[i]);
    if (total > size)
        return 0;

    size_t pos = 0;
    for (int i = depth - 1; i >= 0; --i)
    {
        buf[pos++] = '/';
        size_t l = strlen(comp[i]);
        memcpy(buf + pos, comp[i], l);
        pos += l;
    }
    buf[pos] = '\0';

    return 1;
}