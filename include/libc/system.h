static inline void syscall_exit(int status) {
    asm volatile(
        "mov $0x00, %%eax\n" // Syscall number for exit
        "mov %0, %%ebx\n"    // Pass status in EBX
        "int $0x80\n"        // Trigger syscall
        :
        : "r"(status)
        : "eax", "ebx");
}

static inline void syscall_printc(char c) {
    asm volatile(
        "mov $0x01, %%eax\n" // Syscall number for printc
        "mov %0, %%ebx\n"    // Pass character in EBX
        "int $0x80\n"        // Trigger syscall
        :
        : "r"(c)
        : "eax", "ebx");
}

static inline void syscall_prints(const char* str) {
    asm volatile(
        "mov $0x02, %%eax\n" // Syscall number for prints
        "mov %0, %%ebx\n"    // Pass string in EBX
        "int $0x80\n"        // Trigger syscall
        :
        : "r"(str)
        : "eax", "ebx");
}

// FILEIO
static inline int syscall_open(const char* path, int mode) {
    int result;
    asm volatile(
        "mov $0x03, %%eax\n" // Syscall number for open
        "mov %1, %%ebx\n"    // Pass path in EBX
        "mov %2, %%ecx\n"    // Pass mode in ECX
        "int $0x80\n"        // Trigger syscall
        "mov %%eax, %0\n"    // Save result to 'result'
        : "=r"(result)
        : "r"(path), "r"(mode)
        : "eax", "ebx", "ecx");
    return result;
}

static inline int syscall_read(int fd, char* buf, int size) {
    int result;
    asm volatile(
        "mov $0x04, %%eax\n" // Syscall number for read
        "mov %1, %%ebx\n"    // Pass file descriptor in EBX
        "mov %2, %%ecx\n"    // Pass buffer in ECX
        "mov %3, %%edx\n"    // Pass size in EDX
        "int $0x80\n"        // Trigger syscall
        "mov %%eax, %0\n"    // Save result to 'result'
        : "=r"(result)
        : "r"(fd), "r"(buf), "r"(size)
        : "eax", "ebx", "ecx", "edx");
    return result;
}

static inline int syscall_write(int fd, const char* buf, int size) {
    int result;
    asm volatile(
        "mov $0x05, %%eax\n" // Syscall number for write
        "mov %1, %%ebx\n"    // Pass file descriptor in EBX
        "mov %2, %%ecx\n"    // Pass buffer in ECX
        "mov %3, %%edx\n"    // Pass size in EDX
        "int $0x80\n"        // Trigger syscall
        "mov %%eax, %0\n"    // Save result to 'result'
        : "=r"(result)
        : "r"(fd), "r"(buf), "r"(size)
        : "eax", "ebx", "ecx", "edx");
    return result;
}

static inline int syscall_close(int fd) {
    int result;
    asm volatile(
        "mov $0x06, %%eax\n" // Syscall number for close
        "mov %1, %%ebx\n"    // Pass file descriptor in EBX
        "int $0x80\n"        // Trigger syscall
        "mov %%eax, %0\n"    // Save result to 'result'
        : "=r"(result)
        : "r"(fd)
        : "eax", "ebx");
    return result;
}