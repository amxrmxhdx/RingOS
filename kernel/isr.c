#include "idt.h"
#include "stdterm.h"
#include "shell.h"
#include "libc/fileio.h"

void isr_handler(struct registers_t *regs) {
    uint32_t int_no = regs->int_no;
    uint32_t syscall_num = regs->eax;
    uint32_t arg1 = regs->ebx;
    uint32_t arg2 = regs->ecx;
    uint32_t arg3 = regs->edx;

    if (int_no == 0x80) {
        switch (syscall_num) {
            // Return to shell
            case 0:
                shell_init();
                shell_run();
                break;
            case 1:
                printChar((char) arg1);
                break;
            case 2:
                print((const char*) arg1);
                break;
            // FILE IO
            case 3: // Open file
                regs->eax = fs_open((const char*)arg1, (int)arg2);
                break;

            case 4: // Read file
                regs->eax = fs_read((int)arg1, (char*)arg2, (int)arg3);
                break;

            case 5: // Write file
                regs->eax = fs_write((int)arg1, (const char*)arg2, (int)arg3);
                break;

            case 6: // Close file
                regs->eax = fs_close((int)arg1);
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