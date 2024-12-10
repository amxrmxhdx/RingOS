#include "types.h"
#include "idt.h"

// IDT entry structure
struct idt_entry {
    uint16_t offset_low;
    uint16_t selector;
    uint8_t zero;
    uint8_t type_attr;
    uint16_t offset_high;
} __attribute__((packed));

// IDT pointer structure
struct idt_ptr {
    uint16_t limit;
    uint32_t base;
} __attribute__((packed));

// IDT and IDT pointer
struct idt_entry idt[256];
struct idt_ptr idtp;

extern void idt_flush(uint32_t);

// Helper function to set an IDT gate
void set_idt_gate(int num, uint32_t base, uint16_t sel, uint8_t flags) {
    idt[num].offset_low = base & 0xFFFF;
    idt[num].selector = sel;
    idt[num].zero = 0;
    idt[num].type_attr = flags;
    idt[num].offset_high = (base >> 16) & 0xFFFF;
}

// Initialize the IDT
void init_idt() {
    idtp.limit = sizeof(idt) - 1;
    idtp.base = (uint32_t)&idt;

    // Clear the IDT
    for (int i = 0; i < 256; i++) {
        set_idt_gate(i, 0, 0x08, 0);
    }

    // Set specific ISRs
    extern void isr0();
    extern void isr80();
    set_idt_gate(0, (uint32_t)isr0, 0x08, 0x8E);   // Divide-by-zero
    set_idt_gate(0x80, (uint32_t)isr80, 0x08, 0x8E); // Syscall

    // Load the IDT
    idt_flush((uint32_t)&idtp);
}
