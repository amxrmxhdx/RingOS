#include "../include/vga.h"
#include "../include/keyboard.h"
#include "../include/fat32.h"
#include "../include/libc/system.h"
#include "../include/string.h"

#define EDITOR_WIDTH 80
#define EDITOR_HEIGHT 25
#define BUFFER_SIZE (EDITOR_WIDTH * (EDITOR_HEIGHT - 1))
#define COMMAND_BUFFER_SIZE 128

static char editor_buffer[BUFFER_SIZE];
static size_t cursor_x = 0, cursor_y = 0;
static bool insert_mode = false;
static bool command_mode_active = false;
static char command_buffer[COMMAND_BUFFER_SIZE];
static size_t command_buffer_pos = 0;
static char current_filename[64] = {0};

void editor_render();
void editor_render_status();
void editor_handle_key(char key);
void editor_execute_command();
void editor_load_file(const char *filename);
void editor_save_file(const char *filename);

void editor_render() {
    for (size_t y = 0; y < EDITOR_HEIGHT - 2; y++) {
        for (size_t x = 0; x < EDITOR_WIDTH; x++) {
            size_t pos = y * EDITOR_WIDTH + x;

            char c = editor_buffer[pos];
            if (c) {
                vga_set_cursor_pos(pos);
                vga_putchar(c);
            }
        }
    }

    vga_set_cursor_pos(cursor_y * EDITOR_WIDTH + cursor_x);
}

void editor_render_status() {
    for (size_t y = 0; y < EDITOR_HEIGHT; y++) {
        for (size_t x = 0; x < EDITOR_WIDTH; x++) {
            size_t pos = y * EDITOR_WIDTH + x;
            vga_set_cursor_pos(pos);
            vga_putchar(' ');
        }
    }

    size_t second_last_row_start = (EDITOR_HEIGHT - 2) * EDITOR_WIDTH;
    size_t last_row_start = (EDITOR_HEIGHT - 1) * EDITOR_WIDTH;

    vga_set_cursor_pos(second_last_row_start);
    vga_set_color(VGA_WHITE, VGA_BLUE);
    vga_writestr("STATUS INFO: ");
    if (command_mode_active) {
        vga_writestr("COMMAND MODE");
    } else if (insert_mode) {
        vga_writestr("INSERT MODE");
    } else {
        vga_writestr("NORMAL MODE");
    }

    vga_set_cursor_pos(last_row_start);
    for (size_t x = 0; x < EDITOR_WIDTH; x++) {
        vga_putchar(' ');
    }
    vga_set_cursor_pos(last_row_start);
    if (command_mode_active) {
        vga_putchar(':');
        vga_writestr(command_buffer);
    } else if (insert_mode) {
        vga_writestr("-- INSERT --");
    } else {
        vga_writestr("-- COMMAND --");
        if (current_filename[0]) {
            vga_writestr(" (");
            vga_writestr(current_filename);
            vga_writestr(")");
        }
    }

    vga_set_color(VGA_WHITE, VGA_BLACK);
}

void editor_execute_command() {
    if (strcmp(command_buffer, "w") == 0) {
        if (current_filename[0]) {
            editor_save_file(current_filename);
        }
    } else if (strcmp(command_buffer, "q") == 0) {
        vga_clear();
        syscall_exit(0);
    }

    command_buffer[0] = '\0';
}

void editor_handle_key(char key) {
    if (insert_mode) {
        if (key == 27) {
            insert_mode = false;
            return;
        }

        if (key == '\n') {
            cursor_x = 0;
            cursor_y++;
            if (cursor_y >= EDITOR_HEIGHT - 2) {
                cursor_y = EDITOR_HEIGHT - 3;
            }
            return;
        }

        if (key >= 32 && key <= 126) {
            size_t pos = cursor_y * EDITOR_WIDTH + cursor_x;

            editor_buffer[pos] = key;

            cursor_x++;
            if (cursor_x >= EDITOR_WIDTH) {
                cursor_x = 0;
                cursor_y++;
                if (cursor_y >= EDITOR_HEIGHT - 2) {
                    cursor_y = EDITOR_HEIGHT - 3;
                }
            }
        } else if (key == '\b') {
            if (cursor_x > 0) {
                cursor_x--;
                editor_buffer[cursor_y * EDITOR_WIDTH + cursor_x] = ' ';
            } else if (cursor_y > 0) {
                cursor_y--;
                cursor_x = EDITOR_WIDTH - 1;
                editor_buffer[cursor_y * EDITOR_WIDTH + cursor_x] = ' ';
            }
        }
    } else if (command_mode_active) {
        if (key == '\n') {
            editor_execute_command();
        } else if (key == '\b') {
            if (command_buffer_pos > 0) {
                command_buffer_pos--;
                command_buffer[command_buffer_pos] = '\0';
            }
        } else if (key >= 32 && key <= 126 && command_buffer_pos < COMMAND_BUFFER_SIZE - 1) {
            command_buffer[command_buffer_pos++] = key;
            command_buffer[command_buffer_pos] = '\0';
        }
    } else {
        if (key == 'i') {
            insert_mode = true;
        } else if (key == ':') {
            command_mode_active = true;
            command_buffer_pos = 0;
            command_buffer[0] = '\0';
        }
    }
}

void editor_load_file(const char *filename) {
    uint32_t size = BUFFER_SIZE;
    if (!fat32_read_file(filename, editor_buffer, &size)) {
        for (size_t i = 0; i < BUFFER_SIZE; i++) {
            editor_buffer[i] = 0;
        }
    } else {
        for (size_t i = 0; i < sizeof(current_filename); i++) {
            current_filename[i] = filename[i];
            if (filename[i] == '\0') break;
        }
    }
}

void editor_save_file(const char *filename) {
    uint32_t size = BUFFER_SIZE;
    if (!fat32_write_file(filename, editor_buffer, size)) {
        vga_set_cursor_pos((EDITOR_HEIGHT - 1) * EDITOR_WIDTH);
        vga_set_color(VGA_WHITE, VGA_RED);
        vga_writestr("Error saving file!");
        vga_set_color(VGA_WHITE, VGA_BLACK);
    } else {
        vga_set_cursor_pos((EDITOR_HEIGHT - 1) * EDITOR_WIDTH);
        vga_set_color(VGA_WHITE, VGA_GREEN);
        vga_writestr("File saved successfully!");
        vga_set_color(VGA_WHITE, VGA_BLACK);
    }
}

void editor_run(const char *filename) {
    editor_load_file(filename);
    editor_render_status();
    editor_render();

    while (1) {
        char key = keyboard_read();
        editor_handle_key(key);

        editor_render_status();
        editor_render();
    }
}
