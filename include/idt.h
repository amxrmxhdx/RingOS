#ifndef IDT_H
#define IDT_H

#include "types.h"

// IDT entry structure
typedef struct {
    uint16_t offset_low;    // Lower 16 bits of handler function address
    uint16_t selector;      // Kernel segment selector
    uint8_t  zero;         // Always zero
    uint8_t  flags;        // Type and attributes
    uint16_t offset_high;   // Higher 16 bits of handler function address
} __attribute__((packed)) idt_entry_t;

// IDT pointer structure
typedef struct {
    uint16_t limit;
    uint32_t base;
} __attribute__((packed)) idt_ptr_t;

void init_idt(void);
void set_interrupt_handler(uint8_t num, void (*handler)(void));

#endif