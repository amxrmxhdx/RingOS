#include "../include/vga.h"
#include "../include/io.h"
#include "../include/string.h"

#define VGA_BLACK 0x0
#define VGA_WHITE 0xF
#define VGA_COLOR(fg, bg) ((fg) | (bg << 4))

static uint16_t* const vga_buffer = (uint16_t*)VGA_MEMORY;
static uint8_t vga_color;
static uint16_t cursor_pos;

static uint16_t vga_entry(char c, uint8_t color) {
    uint16_t c16 = c;
    uint16_t color16 = color;
    return c16 | (color16 << 8);
}

void vga_init(void) {
    outb(0x3C2, 0xE7);

    outb(0x3D4, 0x11);
    outb(0x3D5, 0x7F);

    // Force black background (0), white foreground (15)
    vga_color = VGA_COLOR(VGA_WHITE, VGA_BLACK);  // 00001111 in binary
    cursor_pos = 0;

    // Fill entire screen with spaces using our color
    for (int i = 0; i < VGA_WIDTH * VGA_HEIGHT; i++) {
        vga_buffer[i] = vga_entry(' ', vga_color);
    }

    // Reset cursor
    vga_set_cursor_pos(0);
}

void vga_putchar(char c) {
    if (c == '\n') {
        cursor_pos = (cursor_pos / VGA_WIDTH + 1) * VGA_WIDTH;
    }
    else if (c == '\b') {
        if (cursor_pos > 0) {
            cursor_pos--;
            vga_buffer[cursor_pos] = vga_entry(' ', vga_color);
        }
    }
    else {
        vga_buffer[cursor_pos] = vga_entry(c, vga_color);
        cursor_pos++;
    }

    if (cursor_pos >= VGA_WIDTH * VGA_HEIGHT) {
        // Move everything up
        for (int i = 0; i < VGA_HEIGHT - 1; i++) {
            for (int j = 0; j < VGA_WIDTH; j++) {
                vga_buffer[i * VGA_WIDTH + j] = vga_buffer[(i + 1) * VGA_WIDTH + j];
            }
        }
        // Clear last line
        for (int i = 0; i < VGA_WIDTH; i++) {
            vga_buffer[(VGA_HEIGHT - 1) * VGA_WIDTH + i] = vga_entry(' ', vga_color);
        }
        cursor_pos = (VGA_HEIGHT - 1) * VGA_WIDTH;
    }
    vga_set_cursor_pos(cursor_pos);
}

void vga_writestr(const char* str) {
    for (size_t i = 0; str[i] != '\0'; i++) {
        vga_putchar(str[i]);
    }
}

void vga_writehex(uint32_t n) {
    char buf[9];
    for (int i = 0; i < 8; i++) {
        uint8_t nibble = (n >> (4 * (7 - i))) & 0xF;
        buf[i] = (nibble < 10) ? ('0' + nibble) : ('A' + nibble - 10);
    }
    buf[8] = '\0';
    vga_writestr("0x");
    vga_writestr(buf);
}

void vga_clear(void) {
    for (int i = 0; i < VGA_WIDTH * VGA_HEIGHT; i++) {
        vga_buffer[i] = vga_entry(' ', vga_color);
    }
    cursor_pos = 0;
    vga_set_cursor_pos(cursor_pos);
}

void vga_set_cursor_pos(uint16_t pos) {
    cursor_pos = pos;
    outb(0x3D4, 0x0F);
    outb(0x3D5, (uint8_t)(pos & 0xFF));
    outb(0x3D4, 0x0E);
    outb(0x3D5, (uint8_t)((pos >> 8) & 0xFF));
}

void vga_set_color(uint8_t fg, uint8_t bg) {
    vga_color = (bg << 4) | (fg & 0x0F);
}
