## Terminal

### Building the Terminal
1) Compile `user_prog.asm`
```
nasm -f bin user_prog.asm -o user_prog.bin
```
2) Convert the BIN file
```
xxd -i user_prog.bin > user_prog_bin.h
```