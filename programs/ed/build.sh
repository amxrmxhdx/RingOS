gcc -target i686-elf -m32 -nostdlib -nostdinc -fno-builtin -fno-stack-protector \
    -nostartfiles -nodefaultlibs -fno-pic \
    -Wl,-Ttext=0x101000 -Wl,--oformat=binary \
    ed.c -o ed.bin
