#include "../include/loader.h"
#include "../include/fat32.h"
#include "../include/memory.h"
#include "../include/string.h"
#include "../include/vga.h"
#include "../include/stdint.h"

#define PROGRAM_LOAD_ADDR 0x100000  // Example: 1MB mark
#define STACK_SIZE 0x10000          // 64KB stack

bool load_program(const char* filename, program_info_t* info) {
    if (!info) return false;

    info->entry_point = (PROGRAM_LOAD_ADDR + 0xFFF) & ~0xFFF;
    info->stack_pointer = ((PROGRAM_LOAD_ADDR + STACK_SIZE) & ~0xFFF) - 16; // 16-byte aligned

    memset((void*)PROGRAM_LOAD_ADDR, 0, STACK_SIZE);

    uint32_t size = STACK_SIZE - 0x2000;
    if (!fat32_read_file(filename, (void*)info->entry_point, &size)) {
        vga_writestr("Error: Could not read program file\n");
        return false;
    }

    if (size < 16) {
        vga_writestr("Error: Program file too small\n");
        return false;
    }

    info->loaded = true;
    return true;
}

void jump_to_program(uint32_t entry_point, uint32_t stack_pointer) {
    asm volatile(
        "mov %0, %%esp\n"  // Set up new stack
        "push $0\n"        // Dummy return address
        "mov $0, %%ebp\n"  // Clear base pointer
        "jmp *%1\n"        // Jump to program
        :
        : "r"(stack_pointer), "r"(entry_point)
        : "memory"
    );
}
