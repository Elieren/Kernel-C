#include "tasks.h"
#include "kernel/sched/multitask/multitask.h"
#include "kernel/syscall/syscall.h"
#include "kernel/loader/elf_loader.h"
#include "fs/fat16/fs.h"
#include "lib/string/string.h"
#include "drivers/video/video.h"
#include <asm/cpu.h>

extern bool screen_refresh_status;
extern bool graphics_mode;

/* Задача-реапер: бесконечно вызывает reap_zombies() */
void zombie_reaper_task(void)
{
    for (;;)
    {
        reap_zombies();
        halt();
    }
}

void screen_refresh(void)
{
    for (;;)
    {
        if (screen_refresh_status)
        {
            video_update_screen();
            screen_refresh_status = false;
        }
        halt();
    }
}

static void trim_string(char *str)
{
    if (!str)
        return;

    char *start = str;
    while (*start && (*start == ' ' || *start == '\t' || *start == '\n' || *start == '\r'))
        start++;

    char *end = start + strlen(start) - 1;
    while (end >= start && (*end == ' ' || *end == '\t' || *end == '\n' || *end == '\r'))
        end--;

    if (start != str)
        memmove(str, start, end - start + 2);

    str[end - start + 1] = '\0';
}

static int parse_path(const char *path, char *parent_dir_path, char *file_name, char *ext)
{
    if (!path || path[0] != '/')
        return -1;

    // Копируем путь во временный буфер для модификации
    char tmp[256];
    strncpy(tmp, path, sizeof(tmp) - 1);
    tmp[sizeof(tmp) - 1] = '\0';

    // Разбираем путь на токены по разделителю '/'
    char *saveptr = NULL;
    char *token = strtok_r(tmp, "/", &saveptr);

    if (!token)
        return -2; // путь "/" (пустой)

    // Сохраняем позицию начала пути для построения родительского каталога
    char parent_path[256] = "";
    size_t parent_path_len = 0;
    char *last_token = NULL;

    // Перебираем все компоненты пути
    while (token)
    {
        char *next_token = strtok_r(NULL, "/", &saveptr);

        if (next_token)
        {
            // Если это не последний элемент - добавляем его к пути родителя с '/'
            size_t len = strlen(token);
            if (parent_path_len + len + 1 < sizeof(parent_path))
            {
                memcpy(parent_path + parent_path_len, token, len);
                parent_path_len += len;
                parent_path[parent_path_len] = '/';
                parent_path_len++;
                parent_path[parent_path_len] = '\0';
            }
            else
            {
                // Слишком длинный путь родителя
                return -3;
            }
        }
        else
        {
            // last_token - это имя файла (последний элемент)
            last_token = token;
            break;
        }

        token = next_token;
    }

    // Гарантируем, что last_token установлен
    if (!last_token)
        return -2;

    // Записываем путь родительского каталога (без завершающего '/')
    if (parent_path_len > 0 && parent_path[parent_path_len - 1] == '/')
        parent_path[parent_path_len - 1] = '\0';
    else
        parent_path[0] = '\0'; // Корень "/"

    strncpy(parent_dir_path, parent_path, FS_NAME_MAX - 1);
    parent_dir_path[FS_NAME_MAX - 1] = '\0';

    // Разбиваем имя файла и расширение
    char *dot = strchr(last_token, '.');
    if (dot)
    {
        size_t name_len = dot - last_token;
        if (name_len >= FS_NAME_MAX)
            name_len = FS_NAME_MAX - 1;
        strncpy(file_name, last_token, name_len);
        file_name[name_len] = '\0';

        strncpy(ext, dot + 1, FS_EXT_MAX - 1);
        ext[FS_EXT_MAX - 1] = '\0';
    }
    else
    {
        // Нет расширения
        strncpy(file_name, last_token, FS_NAME_MAX - 1);
        file_name[FS_NAME_MAX - 1] = '\0';

        ext[0] = '\0';
    }

    return 0;
}

void load_and_run_from_autorun(void)
{
    // 1. Найти директорию boot_d в корне
    int boot_idx = fs_find_in_dir("boot_d", NULL, FS_ROOT_IDX, NULL);
    if (boot_idx < 0)
        return; // Нет boot_d

    // 2. Найти файл autorun.rc в boot_d
    fs_entry_t autorun_entry;
    int autorun_idx = fs_find_in_dir("autorun", "rc", boot_idx, &autorun_entry);
    if (autorun_idx < 0)
        return; // Нет autorun.rc

    // 3. Выделить буфер под файл ( +1 для 0 терминации )
    size_t size = autorun_entry.size;
    if (size == 0)
        return;

    char *buffer = (char *)malloc(size + 1);
    if (!buffer)
        return; // Ошибка выделения памяти

    // 4. Считать содержимое файла в buffer
    int res = fs_read_file_in_dir("autorun", "rc", boot_idx, buffer, size, NULL);
    if (res != 0)
    {
        free(buffer);
        return;
    }
    buffer[size] = '\0'; // нуль-терминируем

    // 5. Разбить buffer по ';'
    char *saveptr = NULL;
    char *token = strtok_r(buffer, ";", &saveptr);

    while (token)
    {
        trim_string(token);

        if (*token == '\0')
        {
            token = strtok_r(NULL, ";", &saveptr);
            continue;
        }

        // Строки в формате "/dir/file.ext"
        char parent_dir_name[FS_NAME_MAX];
        char file_name[FS_NAME_MAX];
        char ext[FS_EXT_MAX];

        if (parse_path(token, parent_dir_name, file_name, ext) == 0)
        {
            // Найти индекс каталога parent_dir_name в корне
            fs_entry_t parent_dir_entry;
            int parent_idx = fs_find_in_dir(parent_dir_name, NULL, FS_ROOT_IDX, &parent_dir_entry);
            if (parent_idx >= 0 && parent_dir_entry.is_dir)
            {
                // Найти файл file_name.ext в parent_idx
                fs_entry_t file_entry;
                int file_idx = fs_find_in_dir(file_name, ext, parent_idx, &file_entry);
                if (file_idx >= 0)
                {
                    if (file_entry.size == 0)
                    {
                        token = strtok_r(NULL, ";", &saveptr);
                        continue; // Пропустить пустой файл
                    }
                    // Прочитать файл во временный буфер и распарсить как ELF
                    void *file_buf = malloc(file_entry.size);
                    if (file_buf)
                    {
                        if (fs_read_file_in_dir(file_name, ext, parent_idx, file_buf, file_entry.size, NULL) == 0)
                        {
                            elf_image_t image;
                            if (elf_load_image(file_buf, file_entry.size, 0, &image) == ELF_LOAD_OK)
                            {
                                // Запустить задачу с именем файла (без расширения);
                                // точка входа — настоящий e_entry из ELF.
                                utask_create((void (*)(void))(uintptr_t)image.entry,
                                             0, image.image_base, image.image_size, 0, 0, file_name);
                            }
                            /* при ошибке разбора ELF буфер образа не создавался —
                             * освобождать, кроме file_buf ниже, больше нечего */
                        }
                        free(file_buf);
                    }
                }
            }
        }

        token = strtok_r(NULL, ";", &saveptr);
    }

    free(buffer);
}

/* Регистрация всех стартовых задач */
void tasks_init(void)
{
    load_and_run_from_autorun();

    task_create(zombie_reaper_task, 0, "ZombieReap");
    if (graphics_mode)
        task_create(screen_refresh, 0, "ScreenRefresh");
}