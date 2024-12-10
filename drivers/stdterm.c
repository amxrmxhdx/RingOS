//
// Created by Amirmahdi Khamisipour on 30.11.24.
//
#include "../include/stdterm.h"
#include "../include/vga.h"

void print(const char* str) {
  vga_writestr(str);
}

void println(const char* str) {
  vga_writestr(str);
  vga_putchar('\n');
}

void printChar(char c) {
  vga_putchar(c);
}

void clearScreen() {
  vga_clear();
}