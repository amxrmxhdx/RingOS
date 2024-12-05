#include "../include/memory.h"
#include "../include/types.h"
#include "../include/string.h"

// Simple memory allocator
#define HEAP_SIZE 1024*1024  // 1MB heap
static uint8_t heap[HEAP_SIZE];
static size_t heap_ptr = 0;

void init_memory(void) {
    heap_ptr = 0;
    memset(heap, 0, HEAP_SIZE);  // Clear heap on initialization
}

void* kmalloc(size_t size) {
    if (heap_ptr + size > HEAP_SIZE) {
        return NULL;  // Out of memory
    }
    
    void* ptr = &heap[heap_ptr];
    heap_ptr += size;
    // Align to 4 bytes
    heap_ptr = (heap_ptr + 3) & ~3;
    return ptr;
}

void kfree(void* ptr) {
    // This is a very simple allocator that doesn't actually free memory
    // In a real kernel, you'd want to implement proper memory management
    (void)ptr;
}