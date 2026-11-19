#include "elf_loader.h"
#include <asm/elf.h>

#include "mm/malloc/malloc.h"
#include "lib/string/string.h"

#define ELF_MAX_PHNUM 64u
#define ELF_MAX_IMAGE_SIZE (64u * 1024u * 1024u) // 64 МиБ

static int validate_header(const Elf_Ehdr *eh, size_t file_size)
{
    if (file_size < sizeof(Elf_Ehdr))
        return ELF_ERR_TOO_SMALL;

    if (eh->e_ident[EI_MAG0] != ELFMAG0 || eh->e_ident[EI_MAG1] != ELFMAG1 ||
        eh->e_ident[EI_MAG2] != ELFMAG2 || eh->e_ident[EI_MAG3] != ELFMAG3)
        return ELF_ERR_BAD_MAGIC;

    if (eh->e_ident[EI_CLASS] != ELF_ARCH_CLASS)
        return ELF_ERR_BAD_CLASS;

    if (eh->e_ident[EI_DATA] != ELF_ARCH_DATA)
        return ELF_ERR_BAD_ENDIAN;

    if (eh->e_ident[EI_VERSION] != EV_CURRENT)
        return ELF_ERR_BAD_VERSION;

    if (eh->e_machine != ELF_ARCH_MACHINE)
        return ELF_ERR_BAD_MACHINE;

    // ET_DYN не поддерживается: требует динамической линковки.
    if (eh->e_type != ET_EXEC)
        return ELF_ERR_BAD_TYPE;

    if (eh->e_phentsize != 0 && eh->e_phentsize < sizeof(Elf_Phdr))
        return ELF_ERR_BAD_PHDR_TAB;

    if (eh->e_phnum == 0 || eh->e_phnum > ELF_MAX_PHNUM)
        return ELF_ERR_NO_LOAD_SEGS;

    // Проверка: таблица program-заголовков не выходит за файл и не переполняется.
    Elf_Addr phtab_bytes = (Elf_Addr)eh->e_phnum * (Elf_Addr)sizeof(Elf_Phdr);
    if (phtab_bytes / sizeof(Elf_Phdr) != (Elf_Addr)eh->e_phnum)
        return ELF_ERR_BAD_PHDR_TAB;

    if (eh->e_phoff > (Elf_Addr)file_size ||
        phtab_bytes > (Elf_Addr)file_size - eh->e_phoff)
        return ELF_ERR_BAD_PHDR_TAB;

    return ELF_LOAD_OK;
}

int elf_load_image(const void *file_data, size_t file_size,
                   size_t extra_tail_space, elf_image_t *out)
{
    if (!file_data || !out)
        return ELF_ERR_TOO_SMALL;

    const Elf_Ehdr *eh = (const Elf_Ehdr *)file_data;

    int rc = validate_header(eh, file_size);
    if (rc != ELF_LOAD_OK)
        return rc;

    const Elf_Phdr *phdrs =
        (const Elf_Phdr *)((const unsigned char *)file_data + eh->e_phoff);

    // Проход 1: проверить PT_LOAD-сегменты, вычислить границы образа.
    int have_load = 0;
    Elf_Addr min_vaddr = 0;
    Elf_Addr max_end = 0;

    for (Elf_Half i = 0; i < eh->e_phnum; i++)
    {
        const Elf_Phdr *ph = &phdrs[i];

        if (ph->p_type != PT_LOAD)
            continue;

        if (ph->p_filesz > ph->p_memsz)
            return ELF_ERR_BAD_SEGMENT;

        if (ph->p_offset > (Elf_Addr)file_size)
            return ELF_ERR_BAD_SEGMENT;
        if (ph->p_filesz > (Elf_Addr)file_size - ph->p_offset)
            return ELF_ERR_BAD_SEGMENT;

        Elf_Addr seg_end = ph->p_vaddr + (Elf_Addr)ph->p_memsz;
        if (seg_end < ph->p_vaddr)
            return ELF_ERR_OVERFLOW;

        // W^X
        if ((ph->p_flags & PF_W) && (ph->p_flags & PF_X))
            return ELF_ERR_WX_SEGMENT;

        if (!have_load || ph->p_vaddr < min_vaddr)
            min_vaddr = ph->p_vaddr;
        if (seg_end > max_end)
            max_end = seg_end;

        have_load = 1;
    }

    if (!have_load)
        return ELF_ERR_NO_LOAD_SEGS;

    if (max_end < min_vaddr)
        return ELF_ERR_OVERFLOW;

    // образ с границы страницы: по ней выставляются права
    Elf_Addr base_vaddr = min_vaddr & ~(Elf_Addr)(HEAP_PAGE_SIZE - 1);

    Elf_Addr image_span_addr = max_end - base_vaddr;
    if (image_span_addr == 0 || image_span_addr > (Elf_Addr)ELF_MAX_IMAGE_SIZE)
        return ELF_ERR_OVERFLOW;

    size_t image_span = (size_t)image_span_addr;

    if (eh->e_entry < min_vaddr || eh->e_entry >= max_end)
        return ELF_ERR_BAD_ENTRY;

    user_region_t regions[ELF_IMAGE_MAX_REGIONS];
    size_t nregions = 0;

    for (Elf_Half i = 0; i < eh->e_phnum; i++)
    {
        const Elf_Phdr *ph = &phdrs[i];
        if (ph->p_type != PT_LOAD || ph->p_memsz == 0)
            continue;

        if (nregions >= ELF_IMAGE_MAX_REGIONS)
            return ELF_ERR_BAD_SEGMENT;

        regions[nregions].offset = (size_t)(ph->p_vaddr - base_vaddr);
        regions[nregions].size = (size_t)ph->p_memsz;
        regions[nregions].perm = ((ph->p_flags & PF_W) ? USER_PERM_W : 0u) |
                                 ((ph->p_flags & PF_X) ? USER_PERM_X : 0u);
        nregions++;
    }

    // W и X на одной странице
    for (size_t i = 0; i < nregions; i++)
    {
        size_t i_lo = regions[i].offset / HEAP_PAGE_SIZE;
        size_t i_hi = (regions[i].offset + regions[i].size + (HEAP_PAGE_SIZE - 1)) / HEAP_PAGE_SIZE;
        for (size_t j = i + 1; j < nregions; j++)
        {
            size_t j_lo = regions[j].offset / HEAP_PAGE_SIZE;
            size_t j_hi = (regions[j].offset + regions[j].size + (HEAP_PAGE_SIZE - 1)) / HEAP_PAGE_SIZE;
            if (i_lo < j_hi && j_lo < i_hi &&
                (((regions[i].perm & USER_PERM_W) && (regions[j].perm & USER_PERM_X)) ||
                 ((regions[i].perm & USER_PERM_X) && (regions[j].perm & USER_PERM_W))))
                return ELF_ERR_WX_SEGMENT;
        }
    }

    // e_entry — только в исполняемом сегменте
    size_t entry_off = (size_t)(eh->e_entry - base_vaddr);
    int entry_exec = 0;
    for (size_t i = 0; i < nregions; i++)
    {
        if ((regions[i].perm & USER_PERM_X) && entry_off >= regions[i].offset &&
            entry_off - regions[i].offset < regions[i].size)
            entry_exec = 1;
    }
    if (!entry_exec)
        return ELF_ERR_BAD_ENTRY;

    // хвост под argv — с новой страницы
    size_t tail_off = image_span;
    if (extra_tail_space != 0)
    {
        if (image_span > SIZE_MAX - (HEAP_PAGE_SIZE - 1))
            return ELF_ERR_OVERFLOW;
        tail_off = (image_span + (HEAP_PAGE_SIZE - 1)) & ~(size_t)(HEAP_PAGE_SIZE - 1);
    }

    size_t total_size = tail_off + extra_tail_space;
    if (total_size < tail_off) // переполнение от extra_tail_space
        return ELF_ERR_OVERFLOW;
    if (total_size > SIZE_MAX - (HEAP_PAGE_SIZE - 1)) // округление до страниц
        return ELF_ERR_OVERFLOW;

    // целые страницы: образ уходит в ring-3, malloc_pages() обнуляет
    void *image_base = malloc_pages((total_size + (HEAP_PAGE_SIZE - 1)) / HEAP_PAGE_SIZE);
    if (!image_base)
        return ELF_ERR_NO_MEMORY;

    // Проход 2: копировать файловое содержимое сегментов.
    for (Elf_Half i = 0; i < eh->e_phnum; i++)
    {
        const Elf_Phdr *ph = &phdrs[i];
        if (ph->p_type != PT_LOAD)
            continue;

        uint8_t *dst = (uint8_t *)image_base +
                       (size_t)(ph->p_vaddr - base_vaddr);
        const uint8_t *src = (const uint8_t *)file_data + ph->p_offset;

        if (ph->p_filesz > 0)
            memcpy(dst, src, (size_t)ph->p_filesz);
    }

    out->image_base = image_base;
    out->image_size = total_size;
    out->segments_end = tail_off;
    out->entry = (uintptr_t)image_base + entry_off;
    out->nregions = nregions;
    for (size_t i = 0; i < nregions; i++)
        out->regions[i] = regions[i];

    return ELF_LOAD_OK;
}

const char *elf_load_status_str(int status)
{
    switch (status)
    {
    case ELF_LOAD_OK:
        return "ok";
    case ELF_ERR_TOO_SMALL:
        return "file too small to contain an ELF header";
    case ELF_ERR_BAD_MAGIC:
        return "bad ELF magic";
    case ELF_ERR_BAD_CLASS:
        return "ELF class does not match architecture";
    case ELF_ERR_BAD_ENDIAN:
        return "ELF data encoding does not match architecture";
    case ELF_ERR_BAD_VERSION:
        return "unsupported ELF version";
    case ELF_ERR_BAD_MACHINE:
        return "e_machine does not match architecture";
    case ELF_ERR_BAD_TYPE:
        return "not ET_EXEC (static executable)";
    case ELF_ERR_BAD_PHDR_TAB:
        return "program header table out of file bounds";
    case ELF_ERR_NO_LOAD_SEGS:
        return "no PT_LOAD segments";
    case ELF_ERR_BAD_SEGMENT:
        return "PT_LOAD segment out of file bounds";
    case ELF_ERR_OVERFLOW:
        return "integer overflow while sizing image";
    case ELF_ERR_NO_MEMORY:
        return "out of memory";
    case ELF_ERR_BAD_ENTRY:
        return "e_entry outside executable segments";
    case ELF_ERR_WX_SEGMENT:
        return "segment or page both writable and executable (W^X)";
    default:
        return "unknown elf_load_status_t";
    }
}
