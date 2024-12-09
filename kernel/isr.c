#include "idt.h"
#include "stdterm.h"

void isr_handler(struct registers_t *regs) {
    if (regs->int_no == 0) {
        print("Divide-by-zero exception!\n");
        while (1);
    } else if (regs->int_no == 0x80) {
        print("Syscall interrupt!\n");
    } else {
        print("Unhandled interrupt: ");
        // print_hex(regs->int_no);
        print("\n");
    }
}