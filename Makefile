CROSS_COMPILE = i686-elf-
CC = $(CROSS_COMPILE)gcc
LD = $(CROSS_COMPILE)ld
AS = nasm
OBJCOPY = $(CROSS_COMPILE)objcopy
OBJDUMP = $(CROSS_COMPILE)objdump
ASFLAGS = -g -f elf32

CFLAGS = -m32 -g -mpreferred-stack-boundary=2 -ffreestanding -O2 -Wall -Wextra -fno-pic -Iinclude
LDFLAGS = -m elf_i386 -T link.ld -nostdlib

# Directories
SRC_DIR = src
INCLUDE_DIR = include

# Source files
C_SOURCES = $(wildcard kernel/*.c drivers/*.c lib/*.c)
ASM_SOURCES = $(wildcard kernel/*.asm)
HEADERS = $(wildcard $(INCLUDE_DIR)/*.h)

# Object files
OBJ = $(C_SOURCES:.c=.o) $(ASM_SOURCES:.asm=.o)

# Disk image settings
FILESYSTEM_DIR = filesystem
DISK_IMAGE = disk.img
DISK_SIZE_MB = 128

QEMU ?= qemu-system-i386
QEMU_FLAGS ?= -no-reboot -monitor stdio
QEMU_VGA ?= std
USERPROG_SRC ?= programs/hello.c
USERPROG_OUT ?= filesystem/userland/apps/HELLO.BIN

# Main target
all: os.bin objdump.txt $(DISK_IMAGE)

# Generate objdump.txt
objdump.txt: os.bin
	$(OBJDUMP) -DxS $< >$@

# Compile C sources
%.o: %.c $(HEADERS)
	$(CC) $(CFLAGS) -c $< -o $@

# Assemble assembly sources
%.o: %.asm
	$(AS) $(ASFLAGS) $< -o $@

# Link everything together
os.bin: $(OBJ)
	$(LD) $(LDFLAGS) -o $@ $^

# Build filesystem tool (host tool)
tools/mkfs: tools/mkfs.c
	cc -o $@ $<

# Create filesystem image
$(DISK_IMAGE): tools/mkfs
	dd if=/dev/zero of=$(DISK_IMAGE) bs=1M count=$(DISK_SIZE_MB)
	./tools/mkfs $(FILESYSTEM_DIR) $(DISK_IMAGE)

# Clean build files
clean:
	rm -f kernel/*.o drivers/*.o lib/*.o os.bin $(DISK_IMAGE) tools/mkfs

# Run in QEMU
run: os.bin $(DISK_IMAGE)
	$(QEMU) -kernel os.bin -drive file=$(DISK_IMAGE),format=raw,if=ide -vga $(QEMU_VGA) \
		-device isa-debug-exit,iobase=0x501,iosize=0x02 \
		$(QEMU_FLAGS)

# Build one custom user program into the filesystem tree.
userprog:
	./tools/build-userprog.sh $(USERPROG_SRC) $(USERPROG_OUT)

# Build default sample app.
userprog-hello:
	./tools/build-userprog.sh programs/hello.c filesystem/userland/apps/HELLO.BIN
