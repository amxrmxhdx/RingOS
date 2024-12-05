#include "../include/loader.h"
#include "../include/fat32.h"
#include "../include/memory.h"
#include "../include/string.h"
#include "../include/vga.h"

bool load_program(const char* filename, program_info_t* info) {
    if (!info) return false;
    
    // Initialize program info
    info->loaded = false;
    info->entry_point = PROGRAM_LOAD_ADDR;
    info->stack_pointer = PROGRAM_LOAD_ADDR + STACK_SIZE;

    // Clear program memory
    memset((void*)PROGRAM_LOAD_ADDR, 0, STACK_SIZE);

    // Debug: Print file details
    vga_writestr("Loading program at: 0x");
    // Print address in hex
    for (int i = 7; i >= 0; i--) {
        char hex = "0123456789ABCDEF"[(PROGRAM_LOAD_ADDR >> (i * 4)) & 0xF];
        char str[2] = {hex, 0};
        vga_writestr(str);
    }
    vga_writestr("\n");

    // Read the program file
    uint32_t size = 0;
    if (!fat32_read_file(filename, (void*)PROGRAM_LOAD_ADDR, &size)) {
        vga_writestr("Error: Could not read program file\n");
        return false;
    }

    // Debug: Print program size
    vga_writestr("Program size: ");
    char size_str[32];
    int idx = 0;
    uint32_t temp = size;
    do {
        size_str[idx++] = '0' + (temp % 10);
        temp /= 10;
    } while (temp > 0);
    while (idx > 0) {
        char c[2] = {size_str[--idx], 0};
        vga_writestr(c);
    }
    vga_writestr(" bytes\n");

    info->loaded = true;
    return true;
}

__attribute__((naked)) void jump_to_program(uint32_t entry_point, uint32_t stack_pointer) {
    // Save the current GDT and IDT limits
    asm volatile(
        // Set up new stack
        "mov %1, %%esp\n"
        // Save old stack
        "push %%ebp\n"
        // Push return address
        "push $1f\n"
        // Jump to program
        "jmp *%0\n"
        // Return point
        "1:\n"
        // Restore old stack
        "pop %%ebp\n"
        "mov %%ebp, %%esp\n"
        "ret\n"
        :
        : "r"(entry_point), "r"(stack_pointer)
    );
}