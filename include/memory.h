#ifndef MEMORY_H
#define MEMORY_H

#include "../include/types.h"

// Memory allocation functions
void init_memory(void);
void* kmalloc(size_t size);
void kfree(void* ptr);

#endif