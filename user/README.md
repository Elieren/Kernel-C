## Terminal

### Building the Terminal
1) Assemble the startup code (`crt0.s`)
```
nasm -f elf32 crt0.s -o crt0.o
```
2) Compile and link with `terminal.c`
```
gcc -m32 -ffreestanding -nostdlib -nostartfiles -o terminal.elf crt0.o terminal.c
```
3) Convert the ELF file to a header for embedding in the project
```
xxd -i terminal.elf > terminal_bin.h
```