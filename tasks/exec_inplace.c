// tasks/exec_inplace.c
#include "../syscall/syscall.h"
#include "../vga/vga.h"
#include "../fat16/fs.h"
#include "elf.h"
#include <stdint.h>
#include <stddef.h>
#include <limits.h>
#include <string.h>
#include "../malloc/malloc.h"
#include "../malloc/user_malloc.h"

#ifndef PAGE_SIZE
#define PAGE_SIZE 0x1000u
#endif

typedef struct utask_load
{
    void *entry;
    void *user_mem;
    size_t user_mem_size;
} utask_load_t;

static inline uint32_t align_up_u32(uint32_t v, uint32_t a)
{
    return (v + (a - 1)) & ~(a - 1);
}
static inline size_t align_up_size(size_t v, size_t a)
{
    return (v + (a - 1)) & ~(a - 1);
}

/* Simple check that [off, off+len) is inside elf_data_len */
static int check_inside(size_t off, size_t len, size_t elf_len)
{
    if (off > elf_len)
        return 0;
    if (len > elf_len)
        return 0;
    if (off + len > elf_len)
        return 0;
    return 1;
}

/*
 * exec_inplace
 *   Copies ELF segments (elf_data, elf_len) into a user area via user_malloc,
 *   returns adjusted entry point (void *).
 *   DOES NOT call the entry.
 */
utask_load_t exec_inplace(uint8_t *elf_data, size_t elf_len)
{
    utask_load_t result = {0};

    if (!elf_data || elf_len < sizeof(Elf32_Ehdr))
    {
        print_string("Invalid ELF data ptr/len", 2, 2, YELLOW, BLACK);
        return result;
    }

    Elf32_Ehdr *ehdr = (Elf32_Ehdr *)elf_data;
    if (!(ehdr->e_ident[0] == 0x7F && ehdr->e_ident[1] == 'E' &&
          ehdr->e_ident[2] == 'L' && ehdr->e_ident[3] == 'F'))
    {
        print_string("Not an ELF binary!", 2, 3, YELLOW, BLACK);
        return result;
    }

    if (ehdr->e_ident[4] != 1)
    {
        print_string("ELF not 32-bit", 2, 3, YELLOW, BLACK);
        return result;
    }

    if (ehdr->e_machine != 3)
    {
        print_string("Unsupported machine type", 2, 4, YELLOW, BLACK);
        return result;
    }

    if (ehdr->e_phoff == 0 || ehdr->e_phnum == 0)
    {
        print_string("No program headers", 2, 4, YELLOW, BLACK);
        return result;
    }

    if (!check_inside(ehdr->e_phoff, ehdr->e_phnum * sizeof(Elf32_Phdr), elf_len))
    {
        print_string("Bad phdr offset/size", 2, 4, YELLOW, BLACK);
        return result;
    }

    Elf32_Phdr *phdr = (Elf32_Phdr *)(elf_data + ehdr->e_phoff);

    /* Find min and max p_vaddr */
    uint32_t min_vaddr = UINT32_MAX;
    uint32_t max_vaddr = 0;
    int found = 0;
    for (int i = 0; i < ehdr->e_phnum; ++i)
    {
        if (phdr[i].p_type != PT_LOAD)
            continue;
        found = 1;
        if (phdr[i].p_vaddr < min_vaddr)
            min_vaddr = phdr[i].p_vaddr;
        uint32_t end = phdr[i].p_vaddr + (uint32_t)phdr[i].p_memsz;
        if (end > max_vaddr)
            max_vaddr = end;
    }
    if (!found)
    {
        print_string("No PT_LOAD found", 2, 4, YELLOW, BLACK);
        return result;
    }

    /* compute span and allocate via user_malloc */
    uint32_t span = (max_vaddr > min_vaddr) ? (max_vaddr - min_vaddr) : PAGE_SIZE;
    uint32_t span_aligned = align_up_u32(span, PAGE_SIZE);

    void *task_load_base = user_malloc(span_aligned);
    if (!task_load_base)
    {
        print_string("User area exhausted", 2, 5, YELLOW, BLACK);
        return result;
    }

    uint32_t load_delta = (uint32_t)((uintptr_t)task_load_base - min_vaddr);

    /* Copy all PT_LOAD segments to allocated area */
    for (int i = 0; i < ehdr->e_phnum; ++i)
    {
        if (phdr[i].p_type != PT_LOAD)
            continue;

        if (!check_inside(phdr[i].p_offset, phdr[i].p_filesz, elf_len))
        {
            print_string("PHDR points outside ELF", 2, 5, YELLOW, BLACK);
            user_free(task_load_base);
            return result;
        }

        void *dest = (void *)((uintptr_t)(phdr[i].p_vaddr + load_delta));
        void *src = elf_data + phdr[i].p_offset;

        memcpy(dest, src, phdr[i].p_filesz);

        if (phdr[i].p_memsz > phdr[i].p_filesz)
        {
            memset((uint8_t *)dest + phdr[i].p_filesz, 0, phdr[i].p_memsz - phdr[i].p_filesz);
        }
    }

    /* Fill result */
    result.entry = (void *)((uintptr_t)(ehdr->e_entry + load_delta));
    result.user_mem = task_load_base;
    result.user_mem_size = span_aligned;

    return result;
}

/*
 * load_task_entry
 *   Loads ELF from FS into malloc buffer, calls exec_inplace,
 *   frees temporary kernel buffer and returns utask_load_t.
 */
utask_load_t load_task_entry_from_dir(const char *name, const char *ext, int dir_idx)
{
    utask_load_t result = {0};

    if (dir_idx < 0)
    {
        print_string("Bad dir index", 4, 4, RED, BLACK);
        return result;
    }

    fs_entry_t fentry;
    int fidx = fs_find_in_dir(name, ext, dir_idx, &fentry);
    if (fidx < 0)
    {
        print_string("File not found in dir!", 4, 4, RED, BLACK);
        return result;
    }

    size_t file_size = fentry.size;
    if (file_size == 0)
    {
        print_string("File is empty!", 4, 5, RED, BLACK);
        return result;
    }

    uint8_t *elf_buffer = (uint8_t *)malloc(file_size);
    if (!elf_buffer)
    {
        print_string("No memory for ELF!", 4, 5, RED, BLACK);
        return result;
    }

    size_t out_size = 0;
    if (fs_read_file_in_dir(name, ext, dir_idx, elf_buffer, file_size, &out_size) != 0 || out_size != file_size)
    {
        print_string("Error reading ELF!", 4, 5, RED, BLACK);
        free(elf_buffer);
        return result;
    }

    result = exec_inplace(elf_buffer, file_size);
    free(elf_buffer);
    return result;
}

int start_task_from_fs(const char *name, const char *ext, int dir_idx, size_t stack_size)
{
    utask_load_t utask = load_task_entry_from_dir(name, ext, dir_idx);
    if (!utask.entry || !utask.user_mem || utask.user_mem_size == 0)
    {
        print_string("Failed to load task!", 6, 4, RED, BLACK);
        return -1;
    }

    size_t stack_sz = stack_size ? stack_size : 8192;
    utask_create((void (*)(void))utask.entry, stack_sz, utask.user_mem, utask.user_mem_size);
    return 0;
}