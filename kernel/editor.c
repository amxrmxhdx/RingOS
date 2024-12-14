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

static bool editor_exit = false;

void editor_render();
void editor_render_status();
void editor_handle_key(char key);
void test_fat32_write();
void editor_execute_command();
void editor_load_file(const char *filename);
void editor_save_file(const char *filename);

void editor_render() {
    for (size_t y = 0; y < EDITOR_HEIGHT - 2; y++) { // Exclude the last two rows
        size_t line_start = y * EDITOR_WIDTH;

        // Clear the entire line first
        vga_set_cursor_pos(line_start);
        for (size_t x = 0; x < EDITOR_WIDTH; x++) {
            vga_putchar(' '); // Clear with spaces
        }

        // Render the line number
        vga_set_cursor_pos(line_start);
        vga_set_color(VGA_GREEN, VGA_BLACK);
        char line_number[5];
        itoa((int)(y + 1), line_number, 4);
        vga_writestr(line_number);

        // Render the line content
        vga_set_color(VGA_WHITE, VGA_BLACK);
        for (size_t x = 0; x < EDITOR_WIDTH - 5; x++) { // Exclude the line number area
            size_t buffer_pos = y * EDITOR_WIDTH + x;
            char c = editor_buffer[buffer_pos];
            if (c == '\0') break; // Stop rendering at the end of the line
            vga_set_cursor_pos(line_start + x + 5);
            vga_putchar(c); // Render character
        }
    }

    // Move cursor to the editor cursor position
    size_t cursor_pos = cursor_y * EDITOR_WIDTH + cursor_x + 5; // Adjust for line number width
    vga_set_cursor_pos(cursor_pos);
}

void editor_render_status() {
    // Define the start position for the last row
    size_t last_row_start = (EDITOR_HEIGHT - 1) * EDITOR_WIDTH;

    // Clear the last row
    vga_set_cursor_pos(last_row_start);
    for (size_t x = 0; x < EDITOR_WIDTH; x++) {
        vga_putchar(' '); // Clear with spaces
    }

    // Write the mode-specific information on the last row
    vga_set_cursor_pos(last_row_start);
    if (command_mode_active) {
        vga_putchar(':');
        vga_writestr(command_buffer);
    } else if (insert_mode) {
        vga_writestr("-- INSERT --");
    } else {
        vga_writestr("-- NORMAL --");
    }

    // Reset VGA colors
    vga_set_color(VGA_WHITE, VGA_BLACK);
}

void editor_execute_command() {
    if (strcmp(command_buffer, "w") == 0) { // Save file
        if (current_filename[0]) {
            editor_save_file(current_filename);
        } else {
            size_t last_row_start = (EDITOR_HEIGHT - 1) * EDITOR_WIDTH;
            vga_set_cursor_pos(last_row_start);
            vga_set_color(VGA_WHITE, VGA_RED);
            vga_writestr("No filename specified!");
            vga_set_color(VGA_WHITE, VGA_BLACK);
        }
    } else if (strcmp(command_buffer, "q") == 0) { // Quit editor
        editor_exit = true;
        vga_clear();
        syscall_exit(0); // Exit the editor
    } else if (strcmp(command_buffer, "t") == 0) { // Test FAT32 write
        test_fat32_write();
    } else { // Unknown command
        size_t last_row_start = (EDITOR_HEIGHT - 1) * EDITOR_WIDTH;
        vga_set_cursor_pos(last_row_start);
        vga_set_color(VGA_WHITE, VGA_RED);
        vga_writestr("Unknown command!");
        vga_set_color(VGA_WHITE, VGA_BLACK);
    }

    // Clear the command buffer
    command_mode_active = false;
    command_buffer_pos = 0;
    command_buffer[0] = '\0';
}

void editor_handle_key(char key) {
    if (insert_mode) {
        if (key == 27) { // ESC to exit Insert Mode
            insert_mode = false;
            return;
        }

        if (key == '\n') { // Handle Enter key
            cursor_x = 0;
            cursor_y++;
            if (cursor_y >= EDITOR_HEIGHT - 2) {
                cursor_y = EDITOR_HEIGHT - 3;
            }
            return;
        }

        if (key >= 32 && key <= 126) { // Printable characters
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
        } else if (key == '\b') { // Handle Backspace
            if (cursor_x > 0) {
                cursor_x--;
                editor_buffer[cursor_y * EDITOR_WIDTH + cursor_x] = '\0'; // Remove the character
            } else if (cursor_y > 0) {
                cursor_y--;
                cursor_x = 0; // Move to the last character in the previous line
                for (int i = EDITOR_WIDTH - 1; i >= 0; i--) {
                    if (editor_buffer[cursor_y * EDITOR_WIDTH + i] != '\0') {
                        cursor_x = i + 1;
                        break;
                    }
                }
            }
        }
    } else if (command_mode_active) { // Command Mode
        if (key == '\n') { // Execute command on Enter
            editor_execute_command(); // Execute the command
            command_mode_active = false; // Exit Command Mode
            command_buffer_pos = 0; // Clear the command buffer
            command_buffer[0] = '\0';
        } else if (key == '\b') { // Handle Backspace in Command Mode
            if (command_buffer_pos > 0) {
                command_buffer_pos--;
                command_buffer[command_buffer_pos] = '\0';
            }
        } else if (key >= 32 && key <= 126 && command_buffer_pos < COMMAND_BUFFER_SIZE - 1) {
            command_buffer[command_buffer_pos++] = key;
            command_buffer[command_buffer_pos] = '\0'; // Null-terminate the command buffer
        }
    } else {
        if (key == '<') { // Left Arrow
            if (cursor_x > 0) {
                cursor_x--;
            }
        } else if (key == '>') { // Right Arrow
            if (cursor_x < EDITOR_WIDTH - 1) {
                cursor_x++;
            }
        } else if (key == '^') { // Up Arrow
            if (cursor_y > 0) {
                cursor_y--;
            }
        } else if (key == 'v') { // Down Arrow
            if (cursor_y < EDITOR_HEIGHT - 3) { // Prevent cursor from moving into the status bar
                cursor_y++;
            }
        } else if (key == 'i') { // Enter Insert Mode
            insert_mode = true;
        } else if (key == ':') { // Enter Command Mode
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
        strcpy(current_filename, filename);
        current_filename[sizeof(current_filename) - 1] = '\0';
    }
}

void editor_save_file(const char *filename) {
    // Check if the file exists
    fat32_dir_entry_t existing_file;
    bool file_exists = fat32_find_file(filename, &existing_file);

    // Calculate the actual size of the editor buffer
    size_t actual_size = 0;
    for (size_t i = 0; i < BUFFER_SIZE; i++) {
        if (editor_buffer[i] != '\0') {
            actual_size = i + 1; // Include the last non-empty character
        }
    }

    // Write the buffer content to the file
    if (!fat32_write_file(filename, editor_buffer, actual_size)) {
        size_t last_row_start = (EDITOR_HEIGHT - 1) * EDITOR_WIDTH;
        vga_set_cursor_pos(last_row_start);
        vga_set_color(VGA_WHITE, VGA_RED);
        vga_writestr("Error: File write failed!");
        vga_set_color(VGA_WHITE, VGA_BLACK);
    } else {
        size_t last_row_start = (EDITOR_HEIGHT - 1) * EDITOR_WIDTH;
        vga_set_cursor_pos(last_row_start);
        vga_set_color(VGA_WHITE, VGA_GREEN);
        vga_writestr("File saved successfully!");
        vga_set_color(VGA_WHITE, VGA_BLACK);
    }
}

void test_fat32_write() {
    const char *test_filename = "TEST.TXT";
    const char *test_data = "Hello, FAT32!";
    uint32_t test_size = strlen(test_data);

    if (fat32_write_file(test_filename, test_data, test_size)) {
        vga_writestr("Test file written successfully.\n");
    } else {
        vga_writestr("Failed to write test file.\n");
    }
}

void editor_run(const char *filename) {
    editor_exit = false;
    editor_load_file(filename);
    editor_render_status();
    editor_render();

    while (!editor_exit) {
        char key = keyboard_read();
        editor_handle_key(key);

        vga_writestr(filename);
        editor_render_status();
        editor_render();
    }
}
