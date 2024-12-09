#ifndef GDT_H
#define GDT_H

#include "types.h"

struct gdt_entry {
    uint16_t limit_low;  // Segment limit (15:0)
    uint16_t base_low;   // Base address (15:0)
    uint8_t base_middle; // Base address (23:16)
    uint8_t access;      // Access byte
    uint8_t granularity; // Flags and limit (19:16)
    uint8_t base_high;   // Base address (31:24)
} __attribute__((packed));

struct gdt_ptr {
    uint16_t limit;
    uint32_t base;
} __attribute__((packed));

void set_gdt_entry(int num, uint32_t base, uint32_t limit, uint8_t access, uint8_t gran);
void init_gdt();
void gdt_flush();

#endif