#include "../include/stdterm.h"

void start_ (void) {
    print("Test Program in c!!\n");
    __asm__ volatile("int $0x80" : : "a"(0));
}
