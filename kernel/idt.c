#include "idt.h"
#include "string.h"

struct idt_entry {
    uint16_t offset_low;  // Lower 16 bits of handler address
    uint16_t selector;    // Kernel code segment selector
    uint8_t zero;         // Always zero
    uint8_t type_attr;    // Type and attributes
    uint16_t offset_high; // Upper 16 bits of handler address
} __attribute__((packed));

struct idt_ptr {
    uint16_t limit;       // Size of IDT - 1
    uint32_t base;        // Base address of IDT
} __attribute__((packed));

struct idt_entry idt[256];
struct idt_ptr idtp;

extern void idt_flush(uint32_t); // Defined in assembly

void set_idt_gate(int num, uint32_t base, uint16_t sel, uint8_t flags) {
    idt[num].offset_low = (base & 0xFFFF);
    idt[num].selector = sel;
    idt[num].zero = 0;
    idt[num].type_attr = flags;
    idt[num].offset_high = (base >> 16) & 0xFFFF;
}

void init_idt() {
    idtp.limit = sizeof(idt) - 1;
    idtp.base = (uint32_t)&idt;

    memset(&idt, 0, sizeof(idt));

    // Setup ISRs
    extern void isr0();   // Divide-by-zero
    extern void isr80();  // Syscall

    set_idt_gate(0, (uint32_t)isr0, 0x08, 0x8E);  // Divide-by-zero
    set_idt_gate(0x80, (uint32_t)isr80, 0x08, 0x8E); // Syscall

    idt_flush((uint32_t)&idtp);
}
