#ifndef IDT_H
#define IDT_H

#include "types.h"

struct registers_t {
    uint32_t gs, fs, es, ds;
    uint32_t edi, esi, ebp, useless_esp, ebx, edx, ecx, eax;
    uint32_t int_no, err_code;
    uint32_t eip, cs, eflags, useresp, ss;
};

void init_idt();
void set_idt_gate(int num, uint32_t base, uint16_t sel, uint8_t flags);

#endif
