#include "libc/fileio.h"
#include "fat32.h"
#include "string.h"
#include "libc/stdio.h"

static char current_path[256] = "/";

// Initialize the filesystem
bool fs_init() {
    if (!fat32_init()) {
        prints("FAT32 initialization failed.\n");
        return false;
    }

    strcpy(current_path, "/");
    return true;
}

// Open a file
int fs_open(const char* path, int mode) {
    fat32_dir_entry_t entry;
    if (fat32_find_file(path, &entry)) {
        if ((entry.attributes & ATTR_DIRECTORY) && mode != 0) {
            prints("Cannot open a directory in write mode.\n");
            return -1;
        }
        return entry.first_cluster_low | (entry.first_cluster_high << 16); // File descriptor
    }

    if (mode == 1) { // Create a new file if in write mode
        if (!fat32_create_file(path)) {
            prints("Failed to create file.\n");
            return -1;
        }
        if (fat32_find_file(path, &entry)) {
            return entry.first_cluster_low | (entry.first_cluster_high << 16);
        }
    }

    return -1; // File not found or creation failed
}

// Read from a file
int fs_read(int fd, char* buffer, int size) {
    fat32_dir_entry_t entry;
    if (!fat32_find_file(fat32_get_current_path(), &entry)) {
        prints("File not found.\n");
        return -1;
    }

    uint32_t bytes_read = 0;
    if (!fat32_read_file(entry.name, buffer, &bytes_read)) {
        prints("Error reading file.\n");
        return -1;
    }

    return (int)bytes_read;
}

// Write to a file
int fs_write(int fd, const char* buffer, int size) {
    fat32_dir_entry_t entry;
    if (!fat32_find_file(fat32_get_current_path(), &entry)) {
        prints("File not found.\n");
        return -1;
    }

    if (!fat32_write_file(entry.name, buffer, size)) {
        prints("Error writing to file.\n");
        return -1;
    }

    return size; // Number of bytes written
}

// Close a file
int fs_close(int fd) {
    // Placeholder for functionality, as FAT32 does not require explicit closing
    return 0;
}

// Change the current directory
bool fs_chdir(const char* path) {
    if (fat32_change_directory(path)) {
        strcpy(current_path, fat32_get_current_path());
        return true;
    }

    prints("Failed to change directory.\n");
    return false;
}

// List directory contents
bool fs_list_directory(void (*callback)(const char* name, uint32_t size, uint8_t attr)) {
    return fat32_list_directory(callback);
}

// Get the current working directory
const char* fs_getcwd() {
    return current_path;
}

// Create a new file
bool fs_create(const char* path) {
    return fat32_create_file(path);
}

// Delete a file
bool fs_delete(const char* path) {
    return fat32_delete_file(path);
}

// Create a new directory
bool fs_mkdir(const char* path) {
    return fat32_create_directory(path);
}
