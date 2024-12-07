#include "idt.h"
#include "string.h"

#define IDT_SIZE 256

static idt_entry_t idt[IDT_SIZE];
static idt_ptr_t   idt_ptr;

// Helper function to set an IDT entry
static void idt_set_gate(uint8_t num, uint32_t base, uint16_t sel, uint8_t flags) {
    idt[num].offset_low = (base & 0xFFFF);
    idt[num].selector = sel;
    idt[num].zero = 0;
    idt[num].flags = flags;
    idt[num].offset_high = (base >> 16) & 0xFFFF;
}

void init_idt(void) {
    // Set up IDT pointer
    idt_ptr.limit = (sizeof(idt_entry_t) * IDT_SIZE) - 1;
    idt_ptr.base = (uint32_t)&idt;

    // Clear IDT
    memset(&idt, 0, sizeof(idt_entry_t) * IDT_SIZE);

    // Load IDT
    asm volatile("lidt %0" : : "m"(idt_ptr));
}

void set_interrupt_handler(uint8_t num, void (*handler)(void)) {
    idt_set_gate(num, (uint32_t)handler, 0x08, 0x8E); // Present, Ring 0, 32-bit Interrupt Gate
}