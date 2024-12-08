#ifndef IDT_H
#define IDT_H

#include "types.h"

struct idt_entry {
    uint16_t offset_low;    // Lower 16 bits of handler function address
    uint16_t selector;      // Kernel segment selector
    uint8_t zero;          // Always zero
    uint8_t type_attr;     // Type and attributes
    uint16_t offset_high;   // Higher 16 bits of handler function address
} __attribute__((packed));

struct idt_ptr {
    uint16_t limit;
    uint32_t base;
} __attribute__((packed));

void idt_set_gate(uint8_t num, uint32_t handler, uint16_t sel, uint8_t flags);
void idt_load();
void isr_handler(struct registers_t regs);
extern void isr0();
void init_interrupts();

#endif