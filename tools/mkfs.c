#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>

#define SECTOR_SIZE 512
#define FAT_ENTRY_SIZE 4
#define DIR_ENTRY_SIZE 32

// FAT32 filesystem image structure
typedef struct {
    unsigned char buffer[32 * 1024 * 1024];  // 32MB filesystem
    size_t size;
} filesystem_image;

void create_filesystem_image(const char* source_dir, const char* output_file) {
    filesystem_image fs = {0};

    // Initialize filesystem structure
    // TODO: Add FAT32 headers and structures

    // Open source directory
    DIR* dir = opendir(source_dir);
    if (!dir) {
        printf("Error: Cannot open source directory\n");
        return;
    }

    // Process directory entries
    struct dirent* entry;
    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_name[0] == '.') continue;  // Skip hidden files

        char full_path[1024];
        snprintf(full_path, sizeof(full_path), "%s/%s", source_dir, entry->d_name);

        struct stat st;
        if (stat(full_path, &st) == 0) {
            // Process file or directory
            // TODO: Add to filesystem image
        }
    }

    closedir(dir);

    // Write filesystem image to file
    FILE* out = fopen(output_file, "wb");
    if (out) {
        fwrite(fs.buffer, 1, fs.size, out);
        fclose(out);
    }
}

int main(int argc, char** argv) {
    if (argc != 3) {
        printf("Usage: %s <source_dir> <output_file>\n", argv[0]);
        return 1;
    }

    create_filesystem_image(argv[1], argv[2]);
    return 0;
}