#ifndef KERNEL_LOADER_ELF_LOADER_H
#define KERNEL_LOADER_ELF_LOADER_H

#include <stddef.h>
#include <stdint.h>

#include "mm/paging/paging.h"

// Коды результата; ELF_LOAD_OK == 0, ошибки — отрицательные.
typedef enum
{
    ELF_LOAD_OK         =  0,
    ELF_ERR_TOO_SMALL   = -1,  // файл меньше ELF-заголовка
    ELF_ERR_BAD_MAGIC   = -2,  // нет сигнатуры 0x7F 'E' 'L' 'F'
    ELF_ERR_BAD_CLASS   = -3,  // разрядность не совпадает с архитектурой
    ELF_ERR_BAD_ENDIAN  = -4,  // порядок байт не совпадает с архитектурой
    ELF_ERR_BAD_VERSION = -5,  // неверная версия ELF
    ELF_ERR_BAD_MACHINE = -6,  // e_machine не совпадает с архитектурой
    ELF_ERR_BAD_TYPE    = -7,  // не ET_EXEC
    ELF_ERR_BAD_PHDR_TAB= -8,  // таблица program-заголовков выходит за файл
    ELF_ERR_NO_LOAD_SEGS= -9,  // нет PT_LOAD-сегментов
    ELF_ERR_BAD_SEGMENT = -10, // PT_LOAD-сегмент выходит за границы файла
    ELF_ERR_OVERFLOW    = -11, // переполнение при вычислении размера образа
    ELF_ERR_NO_MEMORY   = -12, // malloc() вернул NULL
    ELF_ERR_BAD_ENTRY   = -13, // e_entry не в исполняемом сегменте
    ELF_ERR_WX_SEGMENT  = -14, // W и X на одном сегменте или странице
} elf_load_status_t;

// Максимум непустых PT_LOAD
#define ELF_IMAGE_MAX_REGIONS 16

// Результат успешной загрузки.
typedef struct
{
    void     *image_base;    // начало буфера (по границе страницы)
    size_t    image_size;    // полный размер буфера (сегменты + extra_tail)
    size_t    segments_end;  // смещение хвоста под extra_tail (с границы страницы)
    uintptr_t entry;         // абсолютный адрес точки входа
    user_region_t regions[ELF_IMAGE_MAX_REGIONS]; // права сегментов
    size_t        nregions;
} elf_image_t;

// Загружает PT_LOAD-сегменты в новый буфер из целых страниц (malloc_pages).
// Сегмент или страница с W и X отвергаются: линкуйте сегменты по 4 КиБ.
// extra_tail_space — резерв после сегментов (argv), в отдельных страницах.
// При успехе image_base освобождается через free().
// При ошибке память не выделяется, *out не изменяется.
int elf_load_image(const void *file_data, size_t file_size,
                   size_t extra_tail_space, elf_image_t *out);

// Возвращает строковое описание кода результата.
const char *elf_load_status_str(int status);

#endif // KERNEL_LOADER_ELF_LOADER_H
