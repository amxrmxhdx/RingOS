#include "../include/loader.h"
#include "../include/fat32.h"
#include "../include/memory.h"
#include "../include/string.h"
#include "../include/vga.h"
#include "../include/stdint.h"

bool load_program(const char* filename, program_info_t* info) {
    if (!info) return false;
    
    // Initialize program info - keep program and stack well separated
    info->loaded = false;
    info->entry_point = PROGRAM_LOAD_ADDR + 0x1000;  // Add 4KB offset for safety
    info->stack_pointer = PROGRAM_LOAD_ADDR + STACK_SIZE - 0x1000;  // Leave 4KB safety margin

    vga_writestr("Loading program...\n");

    // Use a much smaller buffer initially for testing
    uint32_t buffer_size = 4096;  // Start with just 4KB
    
    // Clear only the program area
    memset((void*)info->entry_point, 0, buffer_size);

    // Read file with limited size
    uint32_t size = buffer_size;
    if (!fat32_read_file(filename, (void*)info->entry_point, &size)) {
        vga_writestr("Error: Could not read program file\n");
        return false;
    }

    info->loaded = true;
    return true;
}

void jump_to_program(uint32_t entry_point, uint32_t stack_pointer) {
    typedef void (*program_entry)(void);
    program_entry program = (program_entry)entry_point;

    vga_writestr("Jumping to program at 0x");
    for (int i = 7; i >= 0; i--) {
        char hex = "0123456789ABCDEF"[(entry_point >> (i * 4)) & 0xF];
        char str[2] = {hex, 0};
        vga_writestr(str);
    }
    vga_writestr("\n");

    // Just switch stack and jump
    asm volatile(
        "mov %0, %%esp\n"
        "call *%1\n"
        :
        : "r"(stack_pointer), "r"(entry_point)
        : "memory"
    );

    // We shouldn't get here normally - program should exit via interrupt
    vga_writestr("Program returned via interrupt\n");
}