#include <string.h>
#include <vga.h>
#include <keyboard.h>
#include <fat32.h>

#define MAX_CMD_LENGTH 256

static char cmd_buffer[MAX_CMD_LENGTH];
static size_t cmd_index = 0;

static void print_prompt(void);
static void cmd_help(void);
static void cmd_about(void);
static void cmd_clear(void);

static void print_prompt(void) {
    vga_writestr("\n@ ");
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

static void cmd_help(void) {
    vga_writestr("\nAvailable commands:");
    vga_writestr("\n  help   - Show this help message");
    vga_writestr("\n  clear  - Clear the screen");
    vga_writestr("\n  about  - Show system information");
    vga_writestr("\n  ls     - List files in directory");
    vga_writestr("\n  create - Create a new file");
    vga_writestr("\n  delete - Delete a file");
}

static void cmd_about(void) {
    vga_writestr("\nRingOS v0.1");
    vga_writestr("\nA minimal operating system with FAT32 support");
}

static void cmd_clear(void) {
    vga_clear();
    print_prompt();
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
    else if (cmd_index > 0) {
        vga_writestr("\nUnknown command: ");
        vga_writestr(command);
        vga_writestr("\nType 'help' for available commands\n");
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
    vga_writestr("Welcome to RingOS!\n");
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