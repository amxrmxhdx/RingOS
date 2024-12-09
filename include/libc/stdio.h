#ifndef CLIB_STDIO_H
#define CLIB_STDIO_H

#include <types.h>

typedef __builtin_va_list va_list;
#define va_start(v,l) __builtin_va_start(v,l)
#define va_end(v) __builtin_va_end(v)
#define va_arg(v,l) __builtin_va_arg(v,l)

static inline int syscall1(int syscall_num, void* arg) {
    int ret;
    asm volatile(
        "int $0x80"
        : "=a"(ret)
        : "a"(syscall_num), "b"(arg)
        : "memory"
    );
    return ret;
}

static void print_number(int num, int base) {
    static const char digits[] = "0123456789abcdef";
    char buf[32];
    int i = 0;
    
    // Handle negative numbers for decimal
    if (base == 10 && num < 0) {
        vga_putchar('-');
        num = -num;
    }
    
    // Handle case of 0
    if (num == 0) {
        vga_putchar('0');
        return;
    }
    
    // Convert to string in reverse order
    while (num > 0) {
        buf[i++] = digits[num % base];
        num /= base;
    }
    
    // Print in correct order
    while (--i >= 0) {
        vga_putchar(buf[i]);
    }
}

static void print_hex(uint32_t num) {
    static const char digits[] = "0123456789abcdef";
    char buf[32];
    int i = 0;
    
    vga_writestr("0x");
    
    // Handle case of 0
    if (num == 0) {
        vga_putchar('0');
        return;
    }
    
    while (num > 0) {
        buf[i++] = digits[num % 16];
        num /= 16;
    }
    
    while (--i >= 0) {
        vga_putchar(buf[i]);
    }
}

void printf(const char* format, ...) {
    va_list args;
    va_start(args, format);
    
    while (*format) {
        if (*format != '%') {
            vga_putchar(*format);
            format++;
            continue;
        }
        
        format++; // Skip the %
        switch (*format) {
            case 'd': {
                int num = va_arg(args, int);
                print_number(num, 10);
                break;
            }
            
            case 'x': {
                uint32_t num = va_arg(args, uint32_t);
                print_hex(num);
                break;
            }
            
            case 'c': {
                // Note: char is promoted to int when passed through ...
                int c = va_arg(args, int);
                vga_putchar((char)c);
                break;
            }
            
            case 's': {
                const char* str = va_arg(args, const char*);
                if (str) {
                    vga_writestr(str);
                } else {
                    vga_writestr("(null)");
                }
                break;
            }
            
            case '%':
                vga_putchar('%');
                break;
                
            default:
                vga_putchar('%');
                vga_putchar(*format);
                break;
        }
        format++;
    }
    
    va_end(args);
}


#endif // CLIB_STDIO_H
