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

static const char* editor_keywords[] = {
    "int", "char", "void", "return", "if", "else", "for", "while",
    "break", "continue", "static", "const", "struct", "typedef", "enum",
    "unsigned", "signed", "sizeof", "switch", "case", "default", "include",
    "clib_puts", "clib_putc", "clib_open", "clib_read", "clib_write", "clib_close",
    "clib_exit", "main", "true", "false", "NULL"
};

static bool editor_is_ident_char(char c) {
    return (c >= 'a' && c <= 'z') ||
           (c >= 'A' && c <= 'Z') ||
           (c >= '0' && c <= '9') ||
           c == '_';
}

static bool editor_word_equals(const char* s, int len, const char* w) {
    int i = 0;
    while (i < len && w[i]) {
        if (s[i] != w[i]) return false;
        i++;
    }
    return i == len && w[i] == '\0';
}

static bool editor_is_keyword(const char* s, int len) {
    for (size_t i = 0; i < sizeof(editor_keywords) / sizeof(editor_keywords[0]); i++) {
        if (editor_word_equals(s, len, editor_keywords[i])) return true;
    }
    return false;
}

static bool editor_word_has_prefix(const char* word, const char* prefix, int prefix_len) {
    for (int i = 0; i < prefix_len; i++) {
        if (prefix[i] == '\0' || word[i] == '\0' || prefix[i] != word[i]) return false;
    }
    return true;
}

static void editor_try_completion(void) {
    int row = (int)cursor_y;
    int col = (int)cursor_x;
    int start;
    int prefix_len;
    char prefix[32];
    const char* match = NULL;
    int remain;

    if (row < 0 || row >= EDITOR_HEIGHT - 2) return;
    if (col <= 0 || col >= EDITOR_WIDTH - 6) return;

    start = col;
    while (start > 0 && editor_is_ident_char(editor_buffer[row * EDITOR_WIDTH + start - 1])) {
        start--;
    }

    prefix_len = col - start;
    if (prefix_len <= 0) return;
    if (prefix_len >= (int)sizeof(prefix)) prefix_len = (int)sizeof(prefix) - 1;

    for (int i = 0; i < prefix_len; i++) {
        prefix[i] = editor_buffer[row * EDITOR_WIDTH + start + i];
    }
    prefix[prefix_len] = '\0';

    for (size_t i = 0; i < sizeof(editor_keywords) / sizeof(editor_keywords[0]); i++) {
        if (editor_word_has_prefix(editor_keywords[i], prefix, prefix_len)) {
            if ((int)strlen(editor_keywords[i]) > prefix_len) {
                match = editor_keywords[i];
                break;
            }
        }
    }

    if (!match) return;

    remain = (int)strlen(match) - prefix_len;
    for (int i = 0; i < remain; i++) {
        if (cursor_x >= EDITOR_WIDTH - 6) break;
        editor_buffer[row * EDITOR_WIDTH + cursor_x] = match[prefix_len + i];
        cursor_x++;
    }
}

void editor_render();
void editor_render_status();
void editor_handle_key(char key);
void test_fat32_write();
void editor_execute_command();
void editor_load_file(const char *filename);
void editor_save_file(const char *filename);

static void editor_clear_buffer(void) {
    for (size_t i = 0; i < BUFFER_SIZE; i++) {
        editor_buffer[i] = 0;
    }
}

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

        // Render the line content with simple syntax highlighting
        for (size_t x = 0; x < EDITOR_WIDTH - 5; x++) { // Exclude the line number area
            size_t buffer_pos = y * EDITOR_WIDTH + x;
            char c = editor_buffer[buffer_pos];
            if (c == '\0') break; // Stop rendering at the end of the line

            uint8_t fg = VGA_WHITE;

            if (c == '#') {
                fg = VGA_LIGHT_MAGENTA;
            } else if (c == '"' || c == '\'') {
                fg = VGA_LIGHT_BROWN;
            } else if ((c >= '0' && c <= '9')) {
                fg = VGA_LIGHT_CYAN;
            } else if (editor_is_ident_char(c) && !(c >= '0' && c <= '9')) {
                int start = (int)x;
                int len = 0;
                while (start > 0 && editor_is_ident_char(editor_buffer[y * EDITOR_WIDTH + (size_t)start - 1])) {
                    start--;
                }
                while ((start + len) < (int)(EDITOR_WIDTH - 5) &&
                       editor_is_ident_char(editor_buffer[y * EDITOR_WIDTH + (size_t)start + (size_t)len])) {
                    len++;
                }
                if (editor_is_keyword(&editor_buffer[y * EDITOR_WIDTH + (size_t)start], len)) {
                    fg = VGA_LIGHT_BLUE;
                }
            }

            if (c == '/' && x + 1 < EDITOR_WIDTH - 5 && editor_buffer[buffer_pos + 1] == '/') {
                fg = VGA_LIGHT_GREEN;
                for (size_t k = x; k < EDITOR_WIDTH - 5; k++) {
                    char cc = editor_buffer[y * EDITOR_WIDTH + k];
                    if (cc == '\0') break;
                    vga_set_cursor_pos(line_start + k + 5);
                    vga_set_color(VGA_LIGHT_GREEN, VGA_BLACK);
                    vga_putchar(cc);
                }
                break;
            }

            vga_set_cursor_pos(line_start + x + 5);
            vga_set_color(fg, VGA_BLACK);
            vga_putchar(c);
        }
    }

    vga_set_color(VGA_WHITE, VGA_BLACK);

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
        vga_writestr("-- INSERT -- TAB:complete ESC:normal");
    } else {
        vga_writestr("-- NORMAL -- i:insert  :w/:q");
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

        if (key == '\t') { // TAB completion
            editor_try_completion();
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
    static char raw[BUFFER_SIZE * 2];
    uint32_t size = (uint32_t)sizeof(raw);
    size_t row = 0;
    size_t col = 0;

    editor_clear_buffer();

    if (!fat32_read_file(filename, raw, &size)) {
        strcpy(current_filename, filename);
        current_filename[sizeof(current_filename) - 1] = '\0';
        return;
    }

    for (uint32_t i = 0; i < size && row < EDITOR_HEIGHT - 2; i++) {
        char c = raw[i];

        if (c == '\r') {
            continue;
        }

        if (c == '\n') {
            row++;
            col = 0;
            continue;
        }

        if (c == '\t') {
            int spaces = 4 - (int)(col % 4);
            while (spaces-- > 0 && col < EDITOR_WIDTH - 5) {
                editor_buffer[row * EDITOR_WIDTH + col] = ' ';
                col++;
            }
            continue;
        }

        if (c >= 32 && c <= 126) {
            if (col >= EDITOR_WIDTH - 5) {
                row++;
                col = 0;
                if (row >= EDITOR_HEIGHT - 2) break;
            }
            editor_buffer[row * EDITOR_WIDTH + col] = c;
            col++;
        }
    }

    strcpy(current_filename, filename);
    current_filename[sizeof(current_filename) - 1] = '\0';
}

void editor_save_file(const char *filename) {
    static char out[BUFFER_SIZE * 2];
    size_t out_len = 0;
    int last_row = -1;

    for (int r = 0; r < EDITOR_HEIGHT - 2; r++) {
        bool has_any = false;
        for (int c = 0; c < EDITOR_WIDTH - 5; c++) {
            if (editor_buffer[r * EDITOR_WIDTH + c] != '\0') {
                has_any = true;
                break;
            }
        }
        if (has_any) last_row = r;
    }

    for (int r = 0; r <= last_row; r++) {
        int end_col = -1;
        for (int c = 0; c < EDITOR_WIDTH - 5; c++) {
            if (editor_buffer[r * EDITOR_WIDTH + c] != '\0') {
                end_col = c;
            }
        }

        if (end_col >= 0) {
            for (int c = 0; c <= end_col; c++) {
                if (out_len + 1 >= sizeof(out)) break;
                char ch = editor_buffer[r * EDITOR_WIDTH + c];
                out[out_len++] = (ch == '\0') ? ' ' : ch;
            }
        }

        if (r < last_row) {
            if (out_len + 1 >= sizeof(out)) break;
            out[out_len++] = '\n';
        }
    }

    if (!fat32_write_file(filename, out, (uint32_t)out_len)) {
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
