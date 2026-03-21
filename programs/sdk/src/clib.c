#include "clib.h"

void clib_exit(int status) {
    __asm__ volatile(
        "mov $0x00, %%eax\n"
        "mov %0, %%ebx\n"
        "int $0x80\n"
        :
        : "r"(status)
        : "eax", "ebx", "memory"
    );

    for (;;) {
        __asm__ volatile("hlt");
    }
}

void clib_putc(char c) {
    __asm__ volatile(
        "mov $0x01, %%eax\n"
        "mov %0, %%ebx\n"
        "int $0x80\n"
        :
        : "r"((int)c)
        : "eax", "ebx", "memory"
    );
}

void clib_puts(const char* s) {
    __asm__ volatile(
        "mov $0x02, %%eax\n"
        "mov %0, %%ebx\n"
        "int $0x80\n"
        :
        : "r"(s)
        : "eax", "ebx", "memory"
    );
}

int clib_open(const char* path, int mode) {
    int result;
    __asm__ volatile(
        "mov $0x03, %%eax\n"
        "mov %1, %%ebx\n"
        "mov %2, %%ecx\n"
        "int $0x80\n"
        "mov %%eax, %0\n"
        : "=r"(result)
        : "r"(path), "r"(mode)
        : "eax", "ebx", "ecx", "memory"
    );
    return result;
}

int clib_read(int fd, char* buf, int size) {
    int result;
    __asm__ volatile(
        "mov $0x04, %%eax\n"
        "mov %1, %%ebx\n"
        "mov %2, %%ecx\n"
        "mov %3, %%edx\n"
        "int $0x80\n"
        "mov %%eax, %0\n"
        : "=r"(result)
        : "r"(fd), "r"(buf), "r"(size)
        : "eax", "ebx", "ecx", "edx", "memory"
    );
    return result;
}

int clib_write(int fd, const char* buf, int size) {
    int result;
    __asm__ volatile(
        "mov $0x05, %%eax\n"
        "mov %1, %%ebx\n"
        "mov %2, %%ecx\n"
        "mov %3, %%edx\n"
        "int $0x80\n"
        "mov %%eax, %0\n"
        : "=r"(result)
        : "r"(fd), "r"(buf), "r"(size)
        : "eax", "ebx", "ecx", "edx", "memory"
    );
    return result;
}

int clib_close(int fd) {
    int result;
    __asm__ volatile(
        "mov $0x06, %%eax\n"
        "mov %1, %%ebx\n"
        "int $0x80\n"
        "mov %%eax, %0\n"
        : "=r"(result)
        : "r"(fd)
        : "eax", "ebx", "memory"
    );
    return result;
}

size_t clib_strlen(const char* s) {
    size_t n = 0;
    if (!s) return 0;
    while (s[n]) n++;
    return n;
}

void* clib_memcpy(void* dst, const void* src, size_t n) {
    unsigned char* d = (unsigned char*)dst;
    const unsigned char* s = (const unsigned char*)src;
    for (size_t i = 0; i < n; i++) d[i] = s[i];
    return dst;
}

void* clib_memset(void* dst, int value, size_t n) {
    unsigned char* d = (unsigned char*)dst;
    for (size_t i = 0; i < n; i++) d[i] = (unsigned char)value;
    return dst;
}
