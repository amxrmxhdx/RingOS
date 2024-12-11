#include "../include/loader.h"
#include "../include/fat32.h"
#include "../include/memory.h"
#include "../include/string.h"
#include "../include/vga.h"
#include "../include/keyboard.h"
#include "../include/shell.h"

#define PROGRAM_LOAD_ADDR 0x100000  // Example: 1MB mark
#define STACK_SIZE 0x1000          // 64KB stack

bool load_program(const char* filename, program_info_t* info) {
    if (!info) return false;

    vga_writestr("Debug 1: Starting load_program\n");

    // Use a temporary buffer first
    static uint8_t temp_buffer[STACK_SIZE];
    uint32_t size = STACK_SIZE - 0x2000;

    vga_writestr("Debug 2: About to read file\n");

    // Read into temporary buffer first
    if (!fat32_read_file(filename, temp_buffer, &size)) {
        vga_writestr("Error: Could not read program file\n");
        return false;
    }

    vga_writestr("Debug 3: File read complete\n");

    // Now copy to final location
    info->entry_point = 0x101000;
    info->stack_pointer = ((PROGRAM_LOAD_ADDR + STACK_SIZE) & ~0xF) - 16;

    // Copy after we're done with all our setup
    memcpy((void*)info->entry_point, temp_buffer, size);

    vga_writestr("Debug 4: Program copied to final location\n");
    info->loaded = true;
    vga_writestr("Debug 5: Returning from load_program\n");
    return true;
}

void jump_to_program(uint32_t entry_point, uint32_t stack_pointer) {
    vga_writestr("Jumping to program\n");

    uint32_t old_esp;
    uint32_t old_ebp;

    asm volatile(
        // Save current stack state
        "movl %%esp, %0\n"
        "movl %%ebp, %1\n"
        : "=r"(old_esp), "=r"(old_ebp)
        :
        : "memory"
    );

    asm volatile(
        // Set up new stack
        "movl %0, %%esp\n"
        "movl $0, %%ebp\n"
        // Call program
        "call *%1\n"
        // Restore original stack
        "movl %2, %%esp\n"
        "movl %3, %%ebp\n"
        :
        : "r"(stack_pointer),    // %0: new stack pointer
          "r"(entry_point),      // %1: program entry point
          "r"(old_esp),          // %2: old stack pointer
          "r"(old_ebp)           // %3: old base pointer
        : "memory"
    );

    keyboard_init();
    vga_writestr("Program completed, returning to shell\n");
}
