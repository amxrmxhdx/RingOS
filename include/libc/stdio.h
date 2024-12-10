#ifndef CLIB_STDIO_H
#define CLIB_STDIO_H

#include "types.h"
#include "system.h"

typedef char* va_list;

#define VA_ALIGN(t) (((sizeof(t) + sizeof(int) - 1) / sizeof(int)) * sizeof(int))
#define va_start(ap, last) (ap = (char*)&last + VA_ALIGN(last))
#define va_arg(ap, t) (*(t *)((ap += VA_ALIGN(t)) - VA_ALIGN(t)))
#define va_end(ap) (ap = NULL)

static void printc(char c) {
    syscall_printc(c);
}

static void prints(const char* str) {
    syscall_prints(str);
}

static void print_number(int num, int base) {
    static const char digits[] = "0123456789abcdef";
    char buf[32];
    int i = 0;

    // Handle negative numbers for decimal
    unsigned int abs_num = (unsigned int)num;
    if (base == 10 && num < 0) {
        printc('-');
        abs_num = (unsigned int)(-num);
    }

    // Handle case of 0
    if (abs_num == 0) {
        printc('0');
        return;
    }

    // Convert to string in reverse order
    while (abs_num > 0) {
        buf[i++] = digits[abs_num % base];
        abs_num /= base;
    }

    // Print in correct order
    while (--i >= 0) {
        printc(buf[i]);
    }
}

static void print_hex(uint32_t num) {
    static const char digits[] = "0123456789abcdef";
    char buf[32];
    int i = 0;
    
    prints("0x");
    
    // Handle case of 0
    if (num == 0) {
        printc('0');
        return;
    }
    
    while (num > 0) {
        buf[i++] = digits[num % 16];
        num /= 16;
    }
    
    while (--i >= 0) {
        printc(buf[i]);
    }
}

/* void printf(const char* format, ...) {
    va_list args;
    va_start(args, format);
    
    while (*format) {
        if (*format != '%') {
            printc(*format);
            format++;
            continue;
        }
        
        format++; // Skip the %
        switch (*format) {
            case 'c':
                // Use `int` for characters because `char` is promoted to `int`
                int c = va_arg(args, int);
                printc((char)c);
                break;

            case 's':
                // Strings are passed as `const char*`
                const char* str = va_arg(args, const char*);
                if (str) {
                    prints(str);
                } else {
                    prints("(null)");
                }
                break;

            case 'd':
                // Use `int` for integers
                int num = va_arg(args, int);
                print_number(num, 10);
                break;
            case 'x':
                // Use `unsigned int` or `uint32_t` for hexadecimal
                uint32_t num1 = va_arg(args, uint32_t);
                print_hex(num1);
                break;
            case '%':
                printc('%');
                break;
                
            default:
                printc('%');
                printc(*format);
                break;
        }
        format++;
    }
    
    va_end(args);
}
 */
#endif // CLIB_STDIO_H
