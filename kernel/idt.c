#include "idt.h"
#include "string.h"
#include "stdterm.h"
#include "vga.h"

struct idt_entry idt[256];
struct idt_ptr idtp;

void idt_set_gate(uint8_t num, uint32_t handler, uint16_t sel, uint8_t flags) {
    idt[num].offset_low = (handler & 0xFFFF);
    idt[num].selector = sel;    // GDT selector (e.g., 0x08 for code segment)
    idt[num].zero = 0;
    idt[num].type_attr = flags; // 0x8E = present, ring 0, 32-bit interrupt gate
    idt[num].offset_high = (handler >> 16) & 0xFFFF;
}

void syscall_print(const char* str) {
    print(str);
}

void isr_handler(struct registers_t regs) {
    if (regs.int_no == 0x80) {
        isr80_handler(&regs);
    }
}

// Load the IDT
void idt_load() {
    idtp.limit = (sizeof(struct idt_entry) * 256) - 1;
    idtp.base = (uint32_t)&idt;
    
    // Clear out the entire IDT first
    memset(&idt, 0, sizeof(struct idt_entry) * 256);
    
    // Load IDT
    asm volatile ("lidt %0" : : "m"(idtp));
}

// Initialize interrupts
void init_interrupts() {
    
    idt_set_gate(0x00, (uint32_t)isr80, 0x08, 0x8E);  // Divide Error Exception
    idt_load();
}

void isr80_handler(struct registers_t *regs) {
    uint32_t syscall_number = regs->eax;
    uint32_t arg1 = regs->ebx;
    uint32_t arg2 = regs->ecx;
    uint32_t arg3 = regs->edx;

    regs->eax = handle_syscall(syscall_number, arg1, arg2, arg3);
}

uint32_t handle_syscall(uint32_t num, uint32_t arg1, uint32_t arg2, uint32_t arg3) {
    vga_writestr("Got interrupt!\n");
    switch(num) {
        case 0:
            // Empty syscall
            return 42;
        case 1:
            // Print syscall
            vga_writestr((const char*)arg1);
            return 0;
        default:
            // Unknown syscall
            return (uint32_t)-1;
    }
}