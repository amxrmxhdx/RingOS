# Compiler and flags
CC = gcc
AS = nasm
CFLAGS = -m32 -nostdlib -nostdinc -fno-builtin -fno-stack-protector -nostartfiles -nodefaultlibs -Wall -Wextra -c
ASFLAGS = -f elf32
LDFLAGS = -m elf_i386 -T link.ld

# Directories
SRC_DIR = src
INCLUDE_DIR = include
BUILD_DIR = build

# Source files
C_SOURCES = $(wildcard kernel/*.c drivers/*.c lib/*.c)
ASM_SOURCES = $(wildcard kernel/*.asm)
HEADERS = $(wildcard $(INCLUDE_DIR)/*.h)

# Object files
OBJ = ${C_SOURCES:.c=.o} ${ASM_SOURCES:.asm=.o}

# Disk image settings
DISK_IMAGE = disk.img
DISK_SIZE_MB = 32

# Main target
all: os.bin $(DISK_IMAGE)

# Compile C sources
%.o: %.c ${HEADERS}
	${CC} ${CFLAGS} -I$(INCLUDE_DIR) $< -o $@

# Compile assembly sources
%.o: %.asm
	${AS} ${ASFLAGS} $< -o $@

# Link everything together
os.bin: ${OBJ}
	ld ${LDFLAGS} -o $@ $^

# Create disk image
$(DISK_IMAGE):
	dd if=/dev/zero of=$(DISK_IMAGE) bs=1M count=$(DISK_SIZE_MB)
	mkfs.fat -F 32 $(DISK_IMAGE)

# Clean build files
clean:
	rm -f kernel/*.o drivers/*.o lib/*.o *.bin $(DISK_IMAGE)

# Run in QEMU
run: os.bin $(DISK_IMAGE)
	qemu-system-i386 -kernel os.bin -drive file=$(DISK_IMAGE),format=raw,if=ide