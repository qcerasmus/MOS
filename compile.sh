rm -f Bin/*
nasm -f elf32 kernel.asm -o Bin/kasm.o
gcc -m32 -c kernel.c -o Bin/kc.o
ld -m elf_i386 -T link.ld -o kernel Bin/kasm.o Bin/kc.o
qemu-system-i386 -kernel kernel