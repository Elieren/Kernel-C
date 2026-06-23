#ifndef KERNEL_LOADER_ELF_LOADER_H
#define KERNEL_LOADER_ELF_LOADER_H

#include <stddef.h>
#include <stdint.h>

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
    ELF_ERR_BAD_ENTRY   = -13, // e_entry не попадает ни в один PT_LOAD
} elf_load_status_t;

// Результат успешной загрузки.
typedef struct
{
    void     *image_base;    // начало буфера
    size_t    image_size;    // полный размер буфера (сегменты + extra_tail)
    size_t    segments_end;  // смещение конца последнего сегмента
    uintptr_t entry;         // абсолютный адрес точки входа
} elf_image_t;

// Загружает все PT_LOAD-сегменты из file_data[0..file_size) в новый буфер.
// extra_tail_space — резерв после сегментов (например, под argv).
// При успехе *out заполнен, владение image_base переходит вызывающей стороне.
// При ошибке память не выделяется, *out не изменяется.
int elf_load_image(const void *file_data, size_t file_size,
                   size_t extra_tail_space, elf_image_t *out);

// Возвращает строковое описание кода результата.
const char *elf_load_status_str(int status);

#endif // KERNEL_LOADER_ELF_LOADER_H
