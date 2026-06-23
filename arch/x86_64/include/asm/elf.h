#ifndef ARCH_X86_64_ELF_H
#define ARCH_X86_64_ELF_H

#include "kernel/loader/elf_defs.h"

#define ELF_ARCH_MACHINE EM_X86_64
#define ELF_ARCH_CLASS ELFCLASS64
#define ELF_ARCH_DATA ELFDATA2LSB

typedef Elf64_Ehdr Elf_Ehdr;
typedef Elf64_Phdr Elf_Phdr;
typedef Elf64_Addr Elf_Addr;
typedef Elf64_Off Elf_Off;
typedef Elf64_Half Elf_Half;
typedef Elf64_Word Elf_Word;
typedef Elf64_Xword Elf_Xword;

#endif // ARCH_X86_64_ELF_H
