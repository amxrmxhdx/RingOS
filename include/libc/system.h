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