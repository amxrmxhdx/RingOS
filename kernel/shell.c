#include <string.h>
#include <vga.h>
#include <keyboard.h>
#include <fat32.h>
#include <loader.h>

#define MAX_CMD_LENGTH 256

static char cmd_buffer[MAX_CMD_LENGTH];
static size_t cmd_index = 0;

static void print_prompt(void);
static void cmd_help(void);
static void cmd_about(void);
static void cmd_clear(void);

static void print_prompt(void) {
    vga_set_color(VGA_CYAN, VGA_BLACK);
    vga_writestr("user");
    vga_set_color(VGA_MAGENTA, VGA_BLACK);
    vga_writestr("@");
    const char* path = fat32_get_current_path();
    vga_set_color(VGA_CYAN, VGA_BLACK);
    vga_writestr(path);
    vga_set_color(VGA_WHITE, VGA_BLACK);
    vga_writestr("> ");
}

static void ls_callback(const char* name, uint32_t size, uint8_t attr) {
    char size_str[16];
    int len = 0;
    uint32_t temp = size;
    
    // Convert size to string
    do {
        size_str[len++] = '0' + (temp % 10);
        temp /= 10;
    } while (temp > 0);
    size_str[len] = '\0';
    
    // Print file information
    vga_writestr(name);
    vga_writestr("    ");
    while (len > 0) {
        vga_putchar(size_str[--len]);
    }
    vga_writestr(" bytes");
    if (attr & ATTR_DIRECTORY) {
        vga_writestr(" <DIR>");
    }
    vga_putchar('\n');
}

static void cmd_ls(void) {
    vga_writestr("\nReading directory...\n");
    if (!fat32_list_directory(ls_callback)) {
        vga_writestr("Error reading directory. Please try again.\n");
    }
    print_prompt();
}

static void cmd_create(const char* filename) {
    if (!filename || strlen(filename) == 0) {
        vga_writestr("\nError: No filename specified\n");
        vga_writestr("Usage: create <filename>\n");
        print_prompt();
        return;
    }

    if (!fat32_create_file(filename)) {
        vga_writestr("\nError creating file. Please try again.\n");
    } else {
        vga_writestr("\nFile created successfully.\n");
    }
    print_prompt();
}

static void cmd_delete(const char* filename) {
    if (!filename || strlen(filename) == 0) {
        vga_writestr("\nError: No filename specified\n");
        vga_writestr("Usage: delete <filename>\n");
        print_prompt();
        return;
    }

    if (!fat32_delete_file(filename)) {
        vga_writestr("\nError deleting file. Please try again.\n");
    } else {
        vga_writestr("\nFile deleted successfully.\n");
    }
    print_prompt();
}

static void cmd_cd(const char* dirname) {
    if (!dirname || strlen(dirname) == 0) {
        // Just print current directory
        vga_writestr("\nCurrent directory: ");
        vga_writestr(fat32_get_current_path());
        vga_writestr("\n");
        return;
    }

    if (fat32_change_directory(dirname)) {
        // Success - new prompt will show new directory
    } else {
        vga_writestr("\nError: Cannot change to directory '");
        vga_writestr(dirname);
        vga_writestr("'\n");
    }
}

void cmd_cat(const char* filename) {
    if (!filename || !*filename) {
        vga_writestr("Usage: cat <filename>\n");
        return;
    }

    // Create a FAT formatted name
    char fat_name[11];
    memset(fat_name, ' ', 11);
    
    // Copy name part (up to 8 chars)
    const char* dot = filename;
    size_t name_len = 0;
    while (*dot && *dot != '.' && name_len < 8) {
        fat_name[name_len] = (*dot >= 'a' && *dot <= 'z') ? 
                            (*dot - 'a' + 'A') : *dot;
        dot++;
        name_len++;
    }

    // Copy extension if exists
    if (*dot == '.' && dot[1]) {
        dot++; // Skip the dot
        size_t ext_pos = 8;
        while (*dot && ext_pos < 11) {
            fat_name[ext_pos] = (*dot >= 'a' && *dot <= 'z') ? 
                               (*dot - 'a' + 'A') : *dot;
            dot++;
            ext_pos++;
        }
    }

    vga_writestr("Looking for file: '");
    for (int i = 0; i < 11; i++) {
        char c[2] = {fat_name[i], '\0'};
        vga_writestr(c);
    }
    vga_writestr("'\n");

    // Static buffer for file content
    static char buffer[4096];  // 4KB buffer
    uint32_t size = sizeof(buffer) - 1;  // Leave room for null terminator

    // Read file
    if (!fat32_read_file(fat_name, buffer, &size)) {
        vga_writestr("Error: File not found or error reading file (size=");
        
        // Convert size to string for debug output
        char size_str[16];
        int idx = 0;
        uint32_t temp = size;
        do {
            size_str[idx++] = '0' + (temp % 10);
            temp /= 10;
        } while (temp > 0);
        size_str[idx] = '\0';
        
        // Print digits in reverse order
        while (idx > 0) {
            char c[2] = {size_str[--idx], '\0'};
            vga_writestr(c);
        }
        
        vga_writestr(")\n");
        return;
    }

    // Null terminate and print
    buffer[size] = '\0';
    vga_writestr(buffer);
    vga_writestr("\n");
}

void cmd_exec(const char* filename) {
    if (!filename || !*filename) {
        vga_writestr("Usage: exec <filename>\n");
        return;
    }

    // Format filename for FAT32
    char fat_name[12];
    memset(fat_name, ' ', 11);  // Fill with spaces
    fat_name[11] = '\0';
    
    // First, find the dot
    const char* dot = filename;
    size_t name_len = 0;
    while (dot[name_len] && dot[name_len] != '.') {
        name_len++;
    }

    // Copy base name (up to 8 chars)
    size_t copy_len = name_len > 8 ? 8 : name_len;
    for (size_t i = 0; i < copy_len; i++) {
        // Convert to uppercase
        char c = filename[i];
        if (c >= 'a' && c <= 'z') {
            c = c - 'a' + 'A';
        }
        fat_name[i] = c;
    }

    // If we found a dot and there's an extension
    if (dot[name_len] == '.' && dot[name_len + 1]) {
        // Copy extension (up to 3 chars)
        const char* ext = &dot[name_len + 1];
        for (size_t i = 0; i < 3 && ext[i]; i++) {
            // Convert to uppercase
            char c = ext[i];
            if (c >= 'a' && c <= 'z') {
                c = c - 'a' + 'A';
            }
            fat_name[8 + i] = c;
        }
    }

    // Debug output
    vga_writestr("FAT name: [");
    for(int i = 0; i < 11; i++) {
        if(fat_name[i] == ' ') {
            vga_writestr("_");  // Show spaces clearly
        } else {
            char c[2] = {fat_name[i], '\0'};
            vga_writestr(c);
        }
    }
    vga_writestr("]\n");

    program_info_t prog_info;
    if (!load_program(fat_name, &prog_info)) {
        vga_writestr("Error: Could not load program\n");
        return;
    }

    vga_writestr("Program loaded successfully. Executing...\n");
    jump_to_program(prog_info.entry_point, prog_info.stack_pointer);
    vga_writestr("Program execution completed\n");
}

static void cmd_help(void) {
    vga_writestr("\nAvailable commands:");
    vga_writestr("\n  help   - Show this help message");
    vga_writestr("\n  clear  - Clear the screen");
    vga_writestr("\n  about  - Show system information");
    vga_writestr("\n  ls     - List files in directory");
    vga_writestr("\n  create - Create a new file");
    vga_writestr("\n  delete - Delete a file");
    vga_writestr("\n  mkdir  - Create a new directory");
    vga_writestr("\n  writeb - Write binary file (hex format)");
    vga_writestr("\n  readb  - Read binary file (hex format)");
    vga_writestr("\n  cd - Change directory");
    vga_writestr("\n  cat - Read file content");
    vga_writestr("\n  exec - Execute a binary");
    vga_writestr("\n");
    print_prompt();
}

static void cmd_about(void) {
    vga_writestr("\nRingOS v0.1");
    vga_writestr("\nA minimal operating system with FAT32 support");
}

static void cmd_clear(void) {
    vga_clear();
    print_prompt();
}

static void cmd_mkdir(const char* dirname) {
    if (!dirname || strlen(dirname) == 0) {
        vga_writestr("\nError: No directory name specified\n");
        vga_writestr("Usage: mkdir <dirname>\n");
        return;
    }

    if (fat32_create_directory(dirname)) {
        vga_writestr("\nDirectory created successfully\n");
    } else {
        vga_writestr("\nError creating directory\n");
    }
}

static void cmd_write_binary(const char* args) {
    // Example command: writeb filename 48656C6C6F
    char filename[12] = {0};
    char hexdata[256] = {0};
    uint8_t binary[128] = {0};
    uint32_t binary_size = 0;

    // Parse arguments
    int i = 0;
    while (args[i] && args[i] != ' ') {
        if (i < 11) filename[i] = args[i];
        i++;
    }

    while (args[i] == ' ') i++;  // Skip spaces

    // Convert hex string to binary
    int j = 0;
    while (args[i] && j < 255) {
        hexdata[j++] = args[i++];
    }
    hexdata[j] = 0;

    // Convert hex to binary
    size_t hex_len = strlen(hexdata);
    for (size_t idx = 0; idx < hex_len; idx += 2) {
        uint8_t value = 0;
        for (int k = 0; k < 2; k++) {
            value <<= 4;
            char c = hexdata[idx + k];
            if (c >= '0' && c <= '9') value |= c - '0';
            else if (c >= 'A' && c <= 'F') value |= c - 'A' + 10;
            else if (c >= 'a' && c <= 'f') value |= c - 'a' + 10;
        }
        binary[binary_size++] = value;
    }

    if (fat32_write_file(filename, binary, binary_size)) {
        vga_writestr("\nBinary file written successfully\n");
    } else {
        vga_writestr("\nError writing binary file\n");
    }
}

static void cmd_read_binary(const char* filename) {
    uint8_t buffer[4096];
    uint32_t size = sizeof(buffer);

    if (fat32_read_file(filename, buffer, &size)) {
        vga_writestr("\nContent (hex): ");
        for (uint32_t i = 0; i < size; i++) {
            char hex[3];
            hex[0] = "0123456789ABCDEF"[buffer[i] >> 4];
            hex[1] = "0123456789ABCDEF"[buffer[i] & 0xF];
            hex[2] = 0;
            vga_writestr(hex);
        }
        vga_writestr("\n");
    } else {
        vga_writestr("\nError reading binary file\n");
    }
}

void shell_process_command(void) {
    cmd_buffer[cmd_index] = '\0';
    char* command = cmd_buffer;
    char* arg = NULL;

    // Split command and argument
    for (size_t i = 0; i < cmd_index; i++) {
        if (cmd_buffer[i] == ' ') {
            cmd_buffer[i] = '\0';
            arg = &cmd_buffer[i + 1];
            break;
        }
    }

    // Process commands
    if (strcmp(command, "help") == 0) {
        cmd_help();
    }
    else if (strcmp(command, "clear") == 0) {
        cmd_clear();
    }
    else if (strcmp(command, "about") == 0) {
        cmd_about();
    }
    else if (strcmp(command, "ls") == 0) {
        cmd_ls();
    }
    else if (strcmp(command, "create") == 0) {
        cmd_create(arg);
    }
    else if (strcmp(command, "delete") == 0) {
        cmd_delete(arg);
    }
    else if (strcmp(command, "mkdir") == 0) {
        cmd_mkdir(arg);
    }
    else if (strcmp(command, "writeb") == 0 && arg) {
        cmd_write_binary(arg);
    }
    else if (strcmp(command, "readb") == 0 && arg) {
        cmd_read_binary(arg);
    }
    else if (strcmp(command, "cd") == 0) {
        cmd_cd(arg);
    }
    else if (strcmp(command, "cat") == 0) {
        cmd_cat(arg);
    }
    else if (strcmp(command, "exec") == 0) {
        cmd_exec(arg);
    }
    else if (cmd_index > 0) {
        vga_writestr("\nUnknown command: ");
        vga_writestr(command);
        vga_writestr("\nType 'help' for available commands.\n");
        print_prompt();
    }
    else {
        print_prompt();
    }

    cmd_index = 0;
}

void shell_handle_keypress(char c) {
    if (c == '\n') {
        vga_putchar('\n');
        shell_process_command();
    }
    else if (c == '\b' && cmd_index > 0) {
        cmd_index--;
        vga_putchar('\b');
    }
    else if (c && cmd_index < MAX_CMD_LENGTH - 1 && c != '\b') {
        cmd_buffer[cmd_index++] = c;
        vga_putchar(c);
    }
}

void shell_init(void) {
    cmd_index = 0;
    vga_clear();
    vga_set_color(VGA_BLUE, VGA_WHITE);
    vga_writestr("Welcome to RingOS!\n");
    vga_set_color(VGA_WHITE, VGA_BLACK);
    vga_writestr("Type 'help' for available commands.\n");
    print_prompt();
}

void shell_run(void) {
    while (1) {
        char c = keyboard_read();
        if (c) {
            shell_handle_keypress(c);
        }
    }
}