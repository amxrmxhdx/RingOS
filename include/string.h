#ifndef RINGOS_STRING_H
#define RINGOS_STRING_H

#include "types.h"

size_t strlen(const char* str);
void* memset(void* ptr, int value, size_t num);
void* memcpy(void* dest, const void* src, size_t num);
int memcmp(const void* s1, const void* s2, size_t n);
int strcmp(const char* s1, const char* s2);
char* strcpy(char* dest, const char* src);
char* strcat(char* dest, const char* src);
char* strncat(char* dest, const char* src, size_t n);

#endif /* RINGOS_STRING_H */