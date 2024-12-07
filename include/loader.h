#ifndef LOADER_H
#define LOADER_H

#include "../include/types.h"

#define PROGRAM_LOAD_ADDR 0x100000  // Keep at 1MB mark
#define STACK_SIZE 0x8000           // Reduce to 32KB to prevent memory conflicts

typedef struct {
    uint32_t entry_point;
    uint32_t stack_pointer;
    bool loaded;
} program_info_t;

bool load_program(const char* filename, program_info_t* info);
void jump_to_program(uint32_t entry_point, uint32_t stack_pointer);

#endif