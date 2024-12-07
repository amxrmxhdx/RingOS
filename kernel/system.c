#include "../include/system.h"
#include "../include/types.h"
#include "../include/vga.h"
#include "../include/idt.h"

void syscall_handler(void) {
    uint32_t syscall_num;
    
    // Get syscall number from eax
    asm volatile("mov %%eax, %0" : "=r"(syscall_num));
    
    switch(syscall_num) {
        case SYSCALL_EXIT:
            // For now, just return to the kernel
            asm volatile("iret");
            break;
            
        case SYSCALL_PRINT:
            // Get string pointer from ebx
            char* str;
            asm volatile("mov %%ebx, %0" : "=r"(str));
            vga_writestr(str);
            break;
    }
}

void init_syscalls(void) {
    // Set up syscall handler at interrupt 0x80
    set_interrupt_handler(0x80, syscall_handler);
}