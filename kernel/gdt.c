#include "types.h"
#include "gdt.h"

// GDT entry structure
struct gdt_entry {
    uint16_t limit_low;
    uint16_t base_low;
    uint8_t base_middle;
    uint8_t access;
    uint8_t granularity;
    uint8_t base_high;
} __attribute__((packed));

// GDT pointer structure
struct gdt_ptr {
    uint16_t limit;
    uint32_t base;
} __attribute__((packed));

// GDT and GDT pointer
struct gdt_entry gdt[3];
struct gdt_ptr gp;

extern void gdt_flush(uint32_t);

// Helper function to set a GDT entry
void set_gdt_entry(int num, uint32_t base, uint32_t limit, uint8_t access, uint8_t granularity) {
    gdt[num].base_low = base & 0xFFFF;
    gdt[num].base_middle = (base >> 16) & 0xFF;
    gdt[num].base_high = (base >> 24) & 0xFF;

    gdt[num].limit_low = limit & 0xFFFF;
    gdt[num].granularity = (limit >> 16) & 0x0F;
    gdt[num].granularity |= granularity & 0xF0;
    gdt[num].access = access;
}

// Initialize the GDT
void init_gdt() {
    gp.limit = sizeof(gdt) - 1;
    gp.base = (uint32_t)&gdt;

    // Null segment
    set_gdt_entry(0, 0, 0, 0, 0);

    // Code segment
    set_gdt_entry(1, 0, 0xFFFFF, 0x9A, 0xCF);

    // Data segment
    set_gdt_entry(2, 0, 0xFFFFF, 0x92, 0xCF);

    // Load the GDT
    gdt_flush((uint32_t)&gp);
}
