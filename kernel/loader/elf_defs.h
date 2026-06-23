#ifndef KERNEL_ELF_DEFS_H
#define KERNEL_ELF_DEFS_H

#include <stdint.h>

// Базовые типы ELF32
typedef uint32_t Elf32_Addr;
typedef uint32_t Elf32_Off;
typedef uint16_t Elf32_Half;
typedef uint32_t Elf32_Word;
typedef int32_t Elf32_Sword;

// Базовые типы ELF64
typedef uint64_t Elf64_Addr;
typedef uint64_t Elf64_Off;
typedef uint16_t Elf64_Half;
typedef uint32_t Elf64_Word;
typedef int32_t Elf64_Sword;
typedef uint64_t Elf64_Xword;
typedef int64_t Elf64_Sxword;

// e_ident: индексы
#define EI_NIDENT 16
#define EI_MAG0 0
#define EI_MAG1 1
#define EI_MAG2 2
#define EI_MAG3 3
#define EI_CLASS 4 // разрядность: ELFCLASS32 / ELFCLASS64
#define EI_DATA 5  // порядок байт: ELFDATA2LSB / ELFDATA2MSB
#define EI_VERSION 6
#define EI_OSABI 7
#define EI_ABIVERSION 8

// e_ident: магическое число
#define ELFMAG0 0x7fu
#define ELFMAG1 'E'
#define ELFMAG2 'L'
#define ELFMAG3 'F'

// EI_CLASS
#define ELFCLASSNONE 0
#define ELFCLASS32 1
#define ELFCLASS64 2

// EI_DATA
#define ELFDATANONE 0
#define ELFDATA2LSB 1 // little-endian
#define ELFDATA2MSB 2 // big-endian

#define EV_CURRENT 1

// e_type
#define ET_NONE 0
#define ET_REL 1  // перемещаемый
#define ET_EXEC 2 // статический исполняемый
#define ET_DYN 3  // PIE / shared library
#define ET_CORE 4

// e_machine (избранные значения)
#define EM_386 3
#define EM_ARM 40
#define EM_X86_64 62
#define EM_AARCH64 183
#define EM_RISCV 243

// Заголовок ELF32
typedef struct
{
    unsigned char e_ident[EI_NIDENT];
    Elf32_Half e_type;
    Elf32_Half e_machine;
    Elf32_Word e_version;
    Elf32_Addr e_entry; // точка входа
    Elf32_Off e_phoff;  // таблица program-заголовков
    Elf32_Off e_shoff;  // таблица section-заголовков
    Elf32_Word e_flags;
    Elf32_Half e_ehsize;
    Elf32_Half e_phentsize; // размер записи program-заголовка
    Elf32_Half e_phnum;     // число program-заголовков
    Elf32_Half e_shentsize;
    Elf32_Half e_shnum;
    Elf32_Half e_shstrndx;
} __attribute__((packed)) Elf32_Ehdr;

// Заголовок ELF64
typedef struct
{
    unsigned char e_ident[EI_NIDENT];
    Elf64_Half e_type;
    Elf64_Half e_machine;
    Elf64_Word e_version;
    Elf64_Addr e_entry; // точка входа
    Elf64_Off e_phoff;  // таблица program-заголовков
    Elf64_Off e_shoff;  // таблица section-заголовков
    Elf64_Word e_flags;
    Elf64_Half e_ehsize;
    Elf64_Half e_phentsize; // размер записи program-заголовка
    Elf64_Half e_phnum;     // число program-заголовков
    Elf64_Half e_shentsize;
    Elf64_Half e_shnum;
    Elf64_Half e_shstrndx;
} __attribute__((packed)) Elf64_Ehdr;

// p_type
#define PT_NULL 0u
#define PT_LOAD 1u // загружаемый сегмент
#define PT_DYNAMIC 2u
#define PT_INTERP 3u
#define PT_NOTE 4u
#define PT_SHLIB 5u
#define PT_PHDR 6u
#define PT_TLS 7u
#define PT_GNU_EH_FRAME 0x6474e550u
#define PT_GNU_STACK 0x6474e551u
#define PT_GNU_RELRO 0x6474e552u
#define PT_GNU_PROPERTY 0x6474e553u

// p_flags
#define PF_X 1u // исполняемый
#define PF_W 2u // записываемый
#define PF_R 4u // читаемый

// Program-заголовок ELF32
typedef struct
{
    Elf32_Word p_type;
    Elf32_Off p_offset;  // смещение в файле
    Elf32_Addr p_vaddr;  // адрес загрузки
    Elf32_Addr p_paddr;  // физический адрес
    Elf32_Word p_filesz; // размер в файле
    Elf32_Word p_memsz;  // размер в памяти
    Elf32_Word p_flags;
    Elf32_Word p_align;
} __attribute__((packed)) Elf32_Phdr;

// Program-заголовок ELF64 (p_flags идёт после p_type, в отличие от ELF32)
typedef struct
{
    Elf64_Word p_type;
    Elf64_Word p_flags;
    Elf64_Off p_offset;   // смещение в файле
    Elf64_Addr p_vaddr;   // адрес загрузки
    Elf64_Addr p_paddr;   // физический адрес
    Elf64_Xword p_filesz; // размер в файле
    Elf64_Xword p_memsz;  // размер в памяти
    Elf64_Xword p_align;
} __attribute__((packed)) Elf64_Phdr;

#endif // KERNEL_ELF_DEFS_H
