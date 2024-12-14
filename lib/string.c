#include "../include/string.h"

size_t strlen(const char* str) {
    const char* s = str;
    while (*s) s++;
    return s - str;
}

void* memset(void* ptr, int value, size_t num) {
    uint8_t* p = (uint8_t*)ptr;
    while (num--) *p++ = (uint8_t)value;
    return ptr;
}

void* memcpy(void* dest, const void* src, size_t num) {
    uint8_t* d = (uint8_t*)dest;
    const uint8_t* s = (const uint8_t*)src;
    while (num--) *d++ = *s++;
    return dest;
}

int memcmp(const void* s1, const void* s2, size_t n) {
    const uint8_t* p1 = s1;
    const uint8_t* p2 = s2;
    while (n--) {
        if (*p1 != *p2) {
            return *p1 - *p2;
        }
        p1++;
        p2++;
    }
    return 0;
}

int strcmp(const char* s1, const char* s2) {
    while (*s1 && (*s1 == *s2)) {
        s1++;
        s2++;
    }
    return *(const unsigned char*)s1 - *(const unsigned char*)s2;
}

char* strcpy(char* dest, const char* src) {
    char* d = dest;
    while ((*d++ = *src++));
    return dest;
}

char* strcat(char* dest, const char* src) {
    char* d = dest + strlen(dest);
    while ((*d++ = *src++));
    return dest;
}

char toupper(char c) {
    if (c >= 'a' && c <= 'z') {
        return c - ('a' - 'A');
    }
    return c;
}

char* strncat(char* dest, const char* src, size_t n) {
    char* d = dest + strlen(dest);
    while (n-- && (*src)) {
        *d++ = *src++;
    }
    *d = '\0';
    return dest;
}

void itoa(int num, char *str, int width) {
    int i = width - 1;
    str[width] = '\0'; // Null-terminate the string

    // Fill the string in reverse with digits
    while (i >= 0) {
        if (num > 0) {
            str[i] = '0' + (num % 10); // Get the last digit
            num /= 10;                // Remove the last digit
        } else {
            str[i] = ' '; // Add padding spaces
        }
        i--;
    }
}
