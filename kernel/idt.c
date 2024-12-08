#include "idt.h"
#include "string.h"
#include "stdterm.h"

struct idt_entry idt[256];
struct idt_ptr idtp;

void idt_set_gate(uint8_t num, uint32_t handler, uint16_t sel, uint8_t flags) {
    idt[num].offset_low = (handler & 0xFFFF);
    idt[num].selector = sel;
    idt[num].zero = 0;
    idt[num].type_attr = flags;
    idt[num].offset_high = (handler >> 16) & 0xFFFF;
}

void syscall_print(const char* str) {
    print(str);
}

void isr_handler(struct registers regs) {
    // Handle syscalls specifically
    if (regs.int_no == 0x80) {
        // Your syscall handler here
        // regs.eax typically contains the syscall number
        // regs.ebx, regs.ecx, regs.edx are typically parameters
        switch (regs.eax) {
            case 1: // PRINT SYSCALL
                syscall_print((const char*)regs.ebx);
                break;
            // Add more syscalls
        }
    } else {
        // Handle other interrupts
        println("Received interrupt: " + regs.int_no);
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

// Assembly interrupt handler wrapper
extern void isr0();

// Initialize interrupts
void init_interrupts() {
    // Initialize IDT
    idt_load();
    
    // Set up interrupt handler for int 0x80 (syscalls)
    idt_set_gate(0x80, (uint32_t)isr0, 0x08, 0x8E);
    
    // Enable interrupts
    asm volatile ("sti");
}