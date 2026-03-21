#include <string.h>
#include <vga.h>
#include <keyboard.h>
#include <fat32.h>
#include <loader.h>
#include "libc/stdio.h"
#include "programs/editor.h"
#include "gui.h"

#define MAX_CMD_LENGTH 256

static char cmd_buffer[MAX_CMD_LENGTH];
static size_t cmd_index = 0;

static void print_prompt(void);
static void cmd_help(void);
static void cmd_about(void);
static void cmd_clear(void);
static void cmd_cc(const char* args);

static bool shell_is_space(char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r';
}

static void shell_skip_spaces(const char** p) {
    while (p && *p && shell_is_space(**p)) (*p)++;
}

static bool shell_next_token(const char** p, char* out, int out_sz) {
    int n = 0;
    if (!p || !*p || !out || out_sz < 2) return false;

    shell_skip_spaces(p);
    if (!**p) return false;

    while (**p && !shell_is_space(**p)) {
        if (n < out_sz - 1) out[n++] = **p;
        (*p)++;
    }
    out[n] = '\0';
    return n > 0;
}

static void shell_make_bin_name(const char* src, char* out, int out_sz) {
    int dot = -1;
    int len = 0;

    if (!src || !*src || !out || out_sz < 5) {
        if (out && out_sz > 0) out[0] = '\0';
        return;
    }

    while (src[len] && len < out_sz - 1) {
        out[len] = src[len];
        if (src[len] == '.') dot = len;
        len++;
    }
    out[len] = '\0';

    if (dot >= 0) {
        out[dot] = '\0';
    }
    strncat(out, ".BIN", out_sz - 1 - (int)strlen(out));
}

static bool mini_c_parse_source(const char* src, char* message, int msg_sz, int* exit_code) {
    const char* p;
    int mlen = 0;

    if (!src || !message || msg_sz < 2 || !exit_code) return false;

    p = src;
    while (*p) {
        if (p[0] == 'c' && p[1] == 'l' && p[2] == 'i' && p[3] == 'b' && p[4] == '_' &&
            p[5] == 'p' && p[6] == 'u' && p[7] == 't' && p[8] == 's') {
            break;
        }
        p++;
    }
    if (!*p) return false;

    while (*p && *p != '(') p++;
    if (*p != '(') return false;
    p++;
    while (*p && shell_is_space(*p)) p++;
    if (*p != '"') return false;
    p++;

    while (*p && *p != '"' && mlen < msg_sz - 1) {
        if (*p == '\\' && p[1]) {
            p++;
            if (*p == 'n') message[mlen++] = '\n';
            else if (*p == 'r') message[mlen++] = '\r';
            else if (*p == 't') message[mlen++] = '\t';
            else if (*p == '\\') message[mlen++] = '\\';
            else if (*p == '"') message[mlen++] = '"';
            else message[mlen++] = *p;
            p++;
            continue;
        }
        message[mlen++] = *p++;
    }
    if (*p != '"') return false;
    message[mlen] = '\0';

    *exit_code = 0;
    p = src;
    while (*p) {
        if (p[0] == 'r' && p[1] == 'e' && p[2] == 't' && p[3] == 'u' && p[4] == 'r' && p[5] == 'n') {
            p += 6;
            while (*p && shell_is_space(*p)) p++;
            {
                int v = 0;
                bool any = false;
                while (*p >= '0' && *p <= '9') {
                    any = true;
                    v = (v * 10) + (*p - '0');
                    p++;
                }
                if (any) {
                    if (v > 255) v = 255;
                    *exit_code = v;
                    break;
                }
            }
        }
        p++;
    }

    return true;
}

static uint32_t mini_c_emit_binary(const char* message, int exit_code, uint8_t* out, uint32_t out_max) {
    uint32_t mlen;
    uint32_t pos = 0;
    uint32_t rel;

    if (!message || !out) return 0;
    mlen = (uint32_t)strlen(message);
    if (out_max < 31u + mlen) return 0;

    out[pos++] = 0xEB; out[pos++] = 0x17; /* jmp short callsite */
    out[pos++] = 0x5B;                     /* pop ebx */
    out[pos++] = 0xB8; out[pos++] = 0x02; out[pos++] = 0x00; out[pos++] = 0x00; out[pos++] = 0x00; /* mov eax,2 */
    out[pos++] = 0xCD; out[pos++] = 0x80; /* int 0x80 */
    out[pos++] = 0xB8; out[pos++] = 0x00; out[pos++] = 0x00; out[pos++] = 0x00; out[pos++] = 0x00; /* mov eax,0 */
    out[pos++] = 0xBB;
    out[pos++] = (uint8_t)(exit_code & 0xFF); out[pos++] = 0x00; out[pos++] = 0x00; out[pos++] = 0x00; /* mov ebx,exit */
    out[pos++] = 0xCD; out[pos++] = 0x80; /* int 0x80 */
    out[pos++] = 0xF4;                     /* hlt */
    out[pos++] = 0xEB; out[pos++] = 0xFE; /* jmp $ */

    out[pos++] = 0xE8;                     /* call popper */
    rel = 0xFFFFFFE4u;                     /* target offset 2 from callsite offset 25 */
    out[pos++] = (uint8_t)(rel & 0xFF);
    out[pos++] = (uint8_t)((rel >> 8) & 0xFF);
    out[pos++] = (uint8_t)((rel >> 16) & 0xFF);
    out[pos++] = (uint8_t)((rel >> 24) & 0xFF);

    memcpy(&out[pos], message, mlen);
    pos += mlen;
    out[pos++] = 0x00;

    return pos;
}

static void to_fat_name11(const char* filename, char fat_name[12]) {
    size_t name_len = 0;
    const char* dot;

    memset(fat_name, ' ', 11);
    fat_name[11] = '\0';
    if (!filename) return;

    dot = filename;
    while (dot[name_len] && dot[name_len] != '.') {
        name_len++;
    }

    for (size_t i = 0; i < name_len && i < 8; i++) {
        char c = filename[i];
        if (c >= 'a' && c <= 'z') c = c - 'a' + 'A';
        fat_name[i] = c;
    }

    if (dot[name_len] == '.' && dot[name_len + 1]) {
        const char* ext = &dot[name_len + 1];
        for (size_t i = 0; i < 3 && ext[i]; i++) {
            char c = ext[i];
            if (c >= 'a' && c <= 'z') c = c - 'a' + 'A';
            fat_name[8 + i] = c;
        }
    }
}

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

static void cmd_ed(const char *filename) {
    if (!filename || strlen(filename) == 0) {
        vga_writestr("\nUsage: ed <filename>\n");
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

    static char buffer[4096];
    uint32_t size = sizeof(buffer) - 1;

    editor_run(fat_name);
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
        // Success - print prompt with new directory
    } else {
        vga_writestr("\nError: Cannot change to directory '");
        vga_writestr(dirname);
        vga_writestr("'\n");
    }
    print_prompt();
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
    const char* exec_name = filename;
    const char* ext = NULL;
    char built_name[32];

    if (!filename || !*filename) {
        vga_writestr("Usage: exec <filename>\n");
        return;
    }

    for (size_t i = 0; filename[i]; i++) {
        if (filename[i] == '.') ext = &filename[i + 1];
    }

    if (ext && ((ext[0] == 'c' || ext[0] == 'C') && ext[1] == '\0')) {
        char fat_src[12];
        static char src_buf[4096];
        static char msg[512];
        static uint8_t bin[1024];
        uint32_t src_sz = sizeof(src_buf) - 1;
        uint32_t bin_sz;
        int exit_code = 0;

        shell_make_bin_name(filename, built_name, (int)sizeof(built_name));
        to_fat_name11(filename, fat_src);

        if (!fat32_read_file(fat_src, src_buf, &src_sz)) {
            vga_writestr("Error: Could not read source for compile\n");
            return;
        }
        src_buf[src_sz] = '\0';

        if (!mini_c_parse_source(src_buf, msg, (int)sizeof(msg), &exit_code)) {
            vga_writestr("Error: Compile failed (need clib_puts(\"...\"))\n");
            return;
        }

        bin_sz = mini_c_emit_binary(msg, exit_code, bin, (uint32_t)sizeof(bin));
        if (bin_sz == 0 || !fat32_write_file(built_name, bin, bin_sz)) {
            vga_writestr("Error: Could not write compiled binary\n");
            return;
        }

        vga_writestr("Auto-compiled ");
        vga_writestr(filename);
        vga_writestr(" -> ");
        vga_writestr(built_name);
        vga_writestr("\n");
        exec_name = built_name;
    }

    // Format filename for FAT32
    char fat_name[12];
    to_fat_name11(exec_name, fat_name);

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
    vga_writestr("Debug B: About to call load_program\n");
    program_info_t prog_info = {0};  // Initialize struct

    if (!load_program(fat_name, &prog_info)) {
        vga_writestr("Error: Could not load program\n");
        return;
    }

    vga_writestr("Debug C: load_program returned successfully\n");
    vga_writestr("Program loaded successfully. Executing...\n");

    vga_writestr("Debug D: About to call jump_to_program\n");
    jump_to_program(prog_info.entry_point, prog_info.stack_pointer);
    vga_writestr("Debug E: Returned from jump_to_program\n");

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
    vga_writestr("\n  mkdir  - Create a new directory");
    vga_writestr("\n  writeb - Write binary file (hex format)");
    vga_writestr("\n  readb  - Read binary file (hex format)");
    vga_writestr("\n  cd     - Change directory");
    vga_writestr("\n  cat    - Read file content");
    vga_writestr("\n  exec   - Execute BIN (or auto-compile .C first)");
    vga_writestr("\n  cc     - Compile simple C to BIN inside OS");
    vga_writestr("\n  ide    - Editor with C highlighting/completion");
    vga_writestr("\n  newapp - Create app template and open editor");
    vga_writestr("\n  gui    - Open graphical desktop (ESC to exit)");
    vga_writestr("\n");
    print_prompt();
}

static void cmd_ide(const char* filename) {
    if (!filename || !*filename) filename = "APP.C";
    cmd_ed(filename);
}

static void cmd_newapp(const char* filename) {
    char fat_name[12];
    const char* tpl =
        "#include \"clib.h\"\n"
        "\n"
        "int main(void) {\n"
        "    clib_puts(\"Hello from RingOS app editor!\\n\");\n"
        "    return 0;\n"
        "}\n";

    if (!filename || !*filename) filename = "APP.C";
    to_fat_name11(filename, fat_name);

    /* fat32_write_file expects a regular name (e.g. APP.C), not FAT-11. */
    if (!fat32_write_file(filename, tpl, (uint32_t)strlen(tpl))) {
        vga_writestr("\nError creating app template\n");
        print_prompt();
        return;
    }

    vga_writestr("\nCreated app template: ");
    vga_writestr(fat_name);
    vga_writestr("\n");

    editor_run(fat_name);
    vga_clear();
    print_prompt();
}

static void cmd_cc(const char* args) {
    char src_name[32];
    char out_name[32];
    char fat_src[12];
    static char src_buf[4096];
    static char msg[512];
    static uint8_t bin[1024];
    uint32_t src_sz = sizeof(src_buf) - 1;
    uint32_t bin_sz;
    int exit_code = 0;
    const char* p = args;

    if (!shell_next_token(&p, src_name, (int)sizeof(src_name))) {
        vga_writestr("\nUsage: cc <source.c> [output.bin]\n");
        vga_writestr("Subset: int main(){ clib_puts(\"text\"); return N; }\n");
        print_prompt();
        return;
    }

    if (!shell_next_token(&p, out_name, (int)sizeof(out_name))) {
        shell_make_bin_name(src_name, out_name, (int)sizeof(out_name));
    }

    to_fat_name11(src_name, fat_src);
    if (!fat32_read_file(fat_src, src_buf, &src_sz)) {
        vga_writestr("\ncc: cannot read source file\n");
        print_prompt();
        return;
    }
    src_buf[src_sz] = '\0';

    if (!mini_c_parse_source(src_buf, msg, (int)sizeof(msg), &exit_code)) {
        vga_writestr("\ncc: parse failed\n");
        vga_writestr("Need clib_puts(\"...\") and optional return N;\n");
        print_prompt();
        return;
    }

    bin_sz = mini_c_emit_binary(msg, exit_code, bin, (uint32_t)sizeof(bin));
    if (bin_sz == 0) {
        vga_writestr("\ncc: internal emit error\n");
        print_prompt();
        return;
    }

    if (!fat32_write_file(out_name, bin, bin_sz)) {
        vga_writestr("\ncc: could not write output file\n");
        print_prompt();
        return;
    }

    vga_writestr("\ncc: built ");
    vga_writestr(out_name);
    vga_writestr("\nrun with: exec ");
    vga_writestr(out_name);
    vga_writestr("\n");
    print_prompt();
}

void get_cpu_name(char *buffer) {
    uint32_t cpu_info[4] = {0};

    __asm__("cpuid"
            : "=a"(cpu_info[0]), "=b"(cpu_info[1]), "=c"(cpu_info[2]), "=d"(cpu_info[3])
            : "a"(0x80000002));
    *(uint32_t *)(buffer) = cpu_info[0];
    *(uint32_t *)(buffer + 4) = cpu_info[1];
    *(uint32_t *)(buffer + 8) = cpu_info[2];
    *(uint32_t *)(buffer + 12) = cpu_info[3];

    __asm__("cpuid"
            : "=a"(cpu_info[0]), "=b"(cpu_info[1]), "=c"(cpu_info[2]), "=d"(cpu_info[3])
            : "a"(0x80000003));
    *(uint32_t *)(buffer + 16) = cpu_info[0];
    *(uint32_t *)(buffer + 20) = cpu_info[1];
    *(uint32_t *)(buffer + 24) = cpu_info[2];
    *(uint32_t *)(buffer + 28) = cpu_info[3];

    __asm__("cpuid"
            : "=a"(cpu_info[0]), "=b"(cpu_info[1]), "=c"(cpu_info[2]), "=d"(cpu_info[3])
            : "a"(0x80000004));
    *(uint32_t *)(buffer + 32) = cpu_info[0];
    *(uint32_t *)(buffer + 36) = cpu_info[1];
    *(uint32_t *)(buffer + 40) = cpu_info[2];
    *(uint32_t *)(buffer + 44) = cpu_info[3];

    buffer[48] = '\0';
}

static void cmd_about(void) {
    char cpu_name[49];
    get_cpu_name(cpu_name);

    vga_set_color(VGA_RED, VGA_BLACK);
    vga_writestr("           ####           ");
    vga_set_color(VGA_WHITE, VGA_BLACK);
    vga_writestr("RingOS - Version 0.1\n");
    vga_set_color(VGA_BLUE, VGA_BLACK);
    vga_writestr("        ##########        ");
    vga_set_color(VGA_WHITE, VGA_BLACK);
    vga_writestr("CPU: ");
    vga_writestr(cpu_name);
    vga_writestr("\n");
    vga_set_color(VGA_GREEN, VGA_BLACK);
    vga_writestr("      ##############      ");
    vga_set_color(VGA_WHITE, VGA_BLACK);
    vga_writestr("Video: VGA 80x25 text mode\n");
    vga_set_color(VGA_MAGENTA, VGA_BLACK);
    vga_writestr("     #####      #####     ");
    vga_set_color(VGA_WHITE, VGA_BLACK);
    vga_writestr("Filesystem: FAT32\n");
    vga_set_color(VGA_CYAN, VGA_BLACK);
    vga_writestr("    #####        #####    \n");
    vga_set_color(VGA_LIGHT_BLUE, VGA_BLACK);
    vga_writestr("    #####        #####    \n");
    vga_set_color(VGA_LIGHT_GREEN, VGA_BLACK);
    vga_writestr("     #####      #####     \n");
    vga_set_color(VGA_LIGHT_CYAN, VGA_BLACK);
    vga_writestr("      ##############      \n");
    vga_set_color(VGA_LIGHT_RED, VGA_BLACK);
    vga_writestr("        ##########        \n");
    vga_set_color(VGA_LIGHT_MAGENTA, VGA_BLACK);
    vga_writestr("           ####           \n");
    vga_set_color(VGA_WHITE, VGA_BLACK);
    print_prompt();
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
    else if (strcmp(command, "cc") == 0) {
        cmd_cc(arg);
    }
    else if (strcmp(command, "int") == 0) {
        prints("Hello from syscall\n");
        // syscall_exit(0);
        print_prompt();
    }
    else if (strcmp(command, "ed") == 0) {
        cmd_ed(arg);
        vga_clear();
        print_prompt();
    }
    else if (strcmp(command, "ide") == 0) {
        cmd_ide(arg);
        vga_clear();
        print_prompt();
    }
    else if (strcmp(command, "newapp") == 0) {
        cmd_newapp(arg);
    }
    else if (strcmp(command, "gui") == 0) {
        gui_run();
        print_prompt();
    }
    else if (strcmp(command, "writetest") == 0) {
        fat32_write_file("test.txt", "Hello Test", 10);
        print_prompt();
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

void shell_execute_line(const char* line) {
    cmd_index = 0;

    if (line) {
        while (line[cmd_index] && cmd_index < MAX_CMD_LENGTH - 1) {
            cmd_buffer[cmd_index] = line[cmd_index];
            cmd_index++;
        }
    }

    shell_process_command();
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

void shell_return_from_program(void) {
    vga_clear();
    shell_init();
    vga_writestr("\nProgram exited\n");
}
