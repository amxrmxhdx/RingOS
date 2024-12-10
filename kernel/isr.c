#include "idt.h"
#include "stdterm.h"

void isr_handler(struct registers_t *regs) {
    uint32_t int_no = regs->int_no;
    uint32_t syscall_num = regs->eax;
    uint32_t arg1 = regs->ebx;

    if (int_no == 0x80) {
        switch (syscall_num) {
            case 1:
                printChar((char) arg1);
                break;
            case 2:
                print((const char*) arg1);
                break;
            default:
                print("Unhandled syscall: ");
                print(syscall_num + "");
                print("\n");
        }
    } else {
        print("Unhandled interrupt: ");
        print(regs->int_no + "");
        print("\n");
    }
}