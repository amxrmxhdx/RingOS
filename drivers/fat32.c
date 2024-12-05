#include "../include/fat32.h"
#include "../include/io.h"
#include "../include/string.h"
#include "../include/vga.h"
#include "../include/ata.h"

static bool is_initialized = false;
static fat32_boot_sector_t boot_sector;
static uint32_t fat_begin_lba;
static uint32_t cluster_begin_lba;
static uint32_t sectors_per_cluster;
static directory_entry_t current_directory;
static directory_entry_t parent_directories[16];
static int directory_depth = 0;

static void uint32_to_str(uint32_t num, char* str) {
    char rev[11];
    int i = 0;
    
    // Handle 0 case
    if (num == 0) {
        str[0] = '0';
        str[1] = '\0';
        return;
    }
    
    // Convert to string in reverse
    while (num != 0) {
        rev[i++] = '0' + (num % 10);
        num = num / 10;
    }
    
    // Reverse the string
    int j;
    for (j = 0; j < i; j++) {
        str[j] = rev[i - 1 - j];
    }
    str[j] = '\0';
}

static bool compare_filename(const char* fat_name, const char* entry_name) {
    // Compare names case-insensitively
    for (int i = 0; i < 11; i++) {
        char c1 = fat_name[i];
        char c2 = entry_name[i];
        
        // Convert both to uppercase for comparison
        if (c1 >= 'a' && c1 <= 'z') c1 = c1 - 'a' + 'A';
        if (c2 >= 'a' && c2 <= 'z') c2 = c2 - 'a' + 'A';
        
        if (c1 != c2) return false;
    }
    return true;
}

static bool compare_names(const char* fat_name, const char* dir_name) {
    // Compare up to the first space or 11 characters
    int i;
    for (i = 0; i < 11; i++) {
        // If we hit a space in the directory entry, ignore remaining spaces in fat_name
        if (dir_name[i] == ' ') {
            while (i < 11 && fat_name[i] == ' ') i++;
            return (i == 11);
        }
        
        // If characters don't match (ignoring case), names don't match
        char c1 = fat_name[i];
        char c2 = dir_name[i];
        if (c1 >= 'a' && c1 <= 'z') c1 = c1 - 'a' + 'A';
        if (c2 >= 'a' && c2 <= 'z') c2 = c2 - 'a' + 'A';
        
        if (c1 != c2) return false;
    }
    return true;
}

static void update_path(void) {
    // Start with root
    current_directory.path[0] = '/';
    current_directory.path[1] = '\0';

    // Build path from parent directories
    for (int i = 0; i < directory_depth; i++) {
        // Don't add another slash if we're already at root
        if (strcmp(current_directory.path, "/") != 0) {
            strcat(current_directory.path, "/");
        }
        
        // Add current directory name, trimming any spaces
        char trimmed_name[12];
        int j;
        for (j = 0; j < 11 && parent_directories[i].name[j] != ' '; j++) {
            trimmed_name[j] = parent_directories[i].name[j];
        }
        trimmed_name[j] = '\0';
        strcat(current_directory.path, trimmed_name);
    }

    // Add current directory to path (if not root)
    if (strcmp(current_directory.name, "/") != 0) {
        // Don't add another slash if we're already at root
        if (strcmp(current_directory.path, "/") != 0) {
            strcat(current_directory.path, "/");
        }
        
        // Add current directory name, trimming any spaces
        char trimmed_name[12];
        int j;
        for (j = 0; j < 11 && current_directory.name[j] != ' '; j++) {
            trimmed_name[j] = current_directory.name[j];
        }
        trimmed_name[j] = '\0';
        strcat(current_directory.path, trimmed_name);
    }
}

bool fat32_init(void) {
    if (!ata_read_sectors(0, 1, &boot_sector)) {
        vga_writestr("Error: Failed to initialize filesystem\n");
        return false;
    }

    if (boot_sector.bytes_per_sector != 512) {
        vga_writestr("Error: Invalid filesystem\n");
        return false;
    }

    fat_begin_lba = boot_sector.reserved_sectors;
    sectors_per_cluster = boot_sector.sectors_per_cluster;
    cluster_begin_lba = fat_begin_lba + (boot_sector.num_fats * boot_sector.fat_size_32);

    current_directory.cluster = boot_sector.root_cluster;
    strcpy(current_directory.name, "/");
    strcpy(current_directory.path, "/");
    directory_depth = 0;

    is_initialized = true;
    return true;
}


uint32_t fat32_get_current_directory(void) {
    return current_directory.cluster;
}

const char* fat32_get_current_path(void) {
    return current_directory.path;
}

static uint32_t cluster_to_lba(uint32_t cluster) {
    return cluster_begin_lba + ((cluster - 2) * sectors_per_cluster);
}

uint32_t fat32_get_next_cluster(uint32_t cluster) {
    uint32_t fat_sector = fat_begin_lba + ((cluster * 4) / 512);
    uint32_t offset = (cluster * 4) % 512;
    uint32_t buffer[128];

    if (!ata_read_sectors(fat_sector, 1, buffer)) {
        return 0x0FFFFFF7;
    }

    return buffer[offset/4] & 0x0FFFFFFF;
}

bool fat32_change_directory(const char* dirname) {
    if (!is_initialized) return false;

    // Special case for root directory
    if (strcmp(dirname, "/") == 0) {
        current_directory.cluster = boot_sector.root_cluster;
        strcpy(current_directory.name, "/");
        directory_depth = 0;
        update_path();
        return true;
    }

    // Handle going up one directory
    if (strcmp(dirname, "..") == 0) {
        if (directory_depth > 0) {
            directory_depth--;
            current_directory = parent_directories[directory_depth];
            update_path();
            return true;
        }
        return false;
    }

    // Handle current directory
    if (strcmp(dirname, ".") == 0) {
        return true;
    }

    // Convert input name to FAT32 8.3 format for comparison
    char fat_name[11];
    memset(fat_name, ' ', 11);
    
    // Find dot in name (if any)
    const char* dot = dirname;
    size_t name_len = 0;
    while (*dot && *dot != '.' && name_len < 8) {
        // Convert to uppercase while copying
        fat_name[name_len] = (*dot >= 'a' && *dot <= 'z') ? 
                            (*dot - 'a' + 'A') : *dot;
        dot++;
        name_len++;
    }

    // If we found a dot and there's an extension
    if (*dot == '.' && dot[1]) {
        dot++; // Skip the dot
        size_t ext_pos = 8;
        while (*dot && ext_pos < 11) {
            // Convert extension to uppercase
            fat_name[ext_pos] = (*dot >= 'a' && *dot <= 'z') ? 
                               (*dot - 'a' + 'A') : *dot;
            dot++;
            ext_pos++;
        }
    }

    // Search for directory in current directory
    uint32_t current_cluster = current_directory.cluster;
    uint8_t buffer[512];
    fat32_dir_entry_t* entry;

    while (current_cluster < 0x0FFFFFF8) {
        uint32_t current_sector = cluster_to_lba(current_cluster);
        
        for (uint32_t i = 0; i < sectors_per_cluster; i++) {
            if (!ata_read_sectors(current_sector + i, 1, buffer)) {
                return false;
            }

            entry = (fat32_dir_entry_t*)buffer;
            for (uint32_t j = 0; j < (SECTOR_SIZE / sizeof(fat32_dir_entry_t)); j++) {
                // Check for end of directory
                if (entry[j].name[0] == 0x00) {
                    return false;
                }

                // Skip deleted entries and volume labels
                if (entry[j].name[0] == 0xE5 || entry[j].attributes == 0x08) {
                    continue;
                }

                // Skip non-directory entries
                if (!(entry[j].attributes & 0x10)) {
                    continue;
                }

                // Compare names
                if (compare_names(fat_name, entry[j].name)) {
                    // Found the directory - push current directory to stack
                    if (directory_depth < 16) {
                        parent_directories[directory_depth++] = current_directory;
                    }

                    // Update current directory
                    uint32_t new_cluster = ((uint32_t)entry[j].first_cluster_high << 16) |
                                          entry[j].first_cluster_low;

                    current_directory.cluster = new_cluster;
                    memcpy(current_directory.name, entry[j].name, 11);
                    current_directory.name[11] = '\0';

                    update_path();
                    return true;
                }
            }
        }
        current_cluster = fat32_get_next_cluster(current_cluster);
        if (current_cluster == 0) {
            return false;
        }
    }

    return false;
}

bool fat32_list_directory(void (*callback)(const char* name, uint32_t size, uint8_t attr)) {
    if (!is_initialized) return false;

    uint32_t current_cluster = current_directory.cluster;
    uint8_t buffer[512];
    fat32_dir_entry_t* entry;

    while (current_cluster < 0x0FFFFFF8) {
        uint32_t current_sector = cluster_to_lba(current_cluster);
        
        for (uint32_t i = 0; i < sectors_per_cluster; i++) {
            if (!ata_read_sectors(current_sector + i, 1, buffer)) {
                return false;
            }

            entry = (fat32_dir_entry_t*)buffer;
            for (uint32_t j = 0; j < (SECTOR_SIZE / sizeof(fat32_dir_entry_t)); j++) {
                // Check for end of directory
                if (entry[j].name[0] == 0x00) {
                    return true;
                }

                // Skip deleted entries, volume labels, and special entries (. and ..)
                if (entry[j].name[0] == 0xE5 || 
                    entry[j].attributes == 0x08 ||
                    entry[j].name[0] == '.') {
                    continue;
                }

                callback((char*)entry[j].name, entry[j].file_size, entry[j].attributes);
            }
        }

        current_cluster = fat32_get_next_cluster(current_cluster);
        if (current_cluster == 0) {
            return false;
        }
    }

    return true;
}

bool fat32_is_directory(const fat32_dir_entry_t* entry) {
    return (entry->attributes & ATTR_DIRECTORY) != 0;
}

bool fat32_create_file(const char* name) {
    if (!is_initialized) return false;

    uint32_t current_cluster = current_directory.cluster;
    uint8_t buffer[512];
    fat32_dir_entry_t* entry;

    while (current_cluster < 0x0FFFFFF8) {
        uint32_t current_sector = cluster_to_lba(current_cluster);

        for (uint32_t i = 0; i < sectors_per_cluster; i++) {
            if (!ata_read_sectors(current_sector + i, 1, buffer)) {
                return false;
            }

            entry = (fat32_dir_entry_t*)buffer;
            for (uint32_t j = 0; j < 16; j++) {
                if (entry[j].name[0] == 0x00 || entry[j].name[0] == 0xE5) {
                    memset(&entry[j], 0, sizeof(fat32_dir_entry_t));
                    memcpy(entry[j].name, name, 11);
                    entry[j].attributes = 0x20;
                    entry[j].file_size = 0;

                    if (!ata_write_sectors(current_sector + i, 1, buffer)) {
                        return false;
                    }
                    return true;
                }
            }
        }
        current_cluster = fat32_get_next_cluster(current_cluster);
    }
    return false;
}

bool fat32_delete_file(const char* name) {
    if (!is_initialized) return false;

    uint32_t current_cluster = current_directory.cluster;
    uint8_t buffer[512];
    fat32_dir_entry_t* entry;

    while (current_cluster < 0x0FFFFFF8) {
        uint32_t current_sector = cluster_to_lba(current_cluster);

        for (uint32_t i = 0; i < sectors_per_cluster; i++) {
            if (!ata_read_sectors(current_sector + i, 1, buffer)) {
                return false;
            }

            entry = (fat32_dir_entry_t*)buffer;
            for (uint32_t j = 0; j < 16; j++) {
                if (entry[j].name[0] != 0x00 && entry[j].name[0] != 0xE5) {
                    if (memcmp(entry[j].name, name, 11) == 0) {
                        entry[j].name[0] = 0xE5;
                        if (!ata_write_sectors(current_sector + i, 1, buffer)) {
                            return false;
                        }
                        return true;
                    }
                }
            }
        }
        current_cluster = fat32_get_next_cluster(current_cluster);
    }
    return false;
}

bool fat32_create_directory(const char* name) {
    if (!is_initialized) return false;

    uint32_t current_cluster = current_directory.cluster;
    uint8_t buffer[512];
    fat32_dir_entry_t* entry;

    while (current_cluster < 0x0FFFFFF8) {
        uint32_t current_sector = cluster_to_lba(current_cluster);

        for (uint32_t i = 0; i < sectors_per_cluster; i++) {
            if (!ata_read_sectors(current_sector + i, 1, buffer)) {
                return false;
            }

            entry = (fat32_dir_entry_t*)buffer;
            for (uint32_t j = 0; j < 16; j++) {
                if (entry[j].name[0] == 0x00 || entry[j].name[0] == 0xE5) {
                    memset(&entry[j], 0, sizeof(fat32_dir_entry_t));
                    memcpy(entry[j].name, name, 11);
                    entry[j].attributes = ATTR_DIRECTORY;
                    entry[j].file_size = 0;

                    if (!ata_write_sectors(current_sector + i, 1, buffer)) {
                        return false;
                    }
                    return true;
                }
            }
        }
        current_cluster = fat32_get_next_cluster(current_cluster);
    }
    return false;
}

uint32_t fat32_allocate_cluster(void) {
    uint32_t buffer[128];  // 512 bytes / 4 bytes per entry

    // Search FAT for a free cluster
    for (uint32_t fat_sector = 0; fat_sector < boot_sector.fat_size_32; fat_sector++) {
        if (!ata_read_sectors(fat_begin_lba + fat_sector, 1, buffer)) {
            return 0;
        }

        for (uint32_t i = 0; i < 128; i++) {
            if (buffer[i] == 0) {
                // Found a free cluster
                uint32_t cluster = fat_sector * 128 + i;
                if (cluster >= 2) {  // Clusters 0 and 1 are reserved
                    // Mark cluster as end of chain
                    if (fat32_write_fat_entry(cluster, 0x0FFFFFF8)) {
                        return cluster;
                    }
                }
            }
        }
    }
    return 0;  // No free clusters
}

bool fat32_write_fat_entry(uint32_t cluster, uint32_t value) {
    uint32_t fat_sector = fat_begin_lba + ((cluster * 4) / 512);
    uint32_t offset = (cluster * 4) % 512;
    uint32_t buffer[128];

    if (!ata_read_sectors(fat_sector, 1, buffer)) {
        return false;
    }

    buffer[offset/4] = value;

    // Write to all FATs
    for (uint32_t fat = 0; fat < boot_sector.num_fats; fat++) {
        uint32_t current_fat_sector = fat_sector + (fat * boot_sector.fat_size_32);
        if (!ata_write_sectors(current_fat_sector, 1, buffer)) {
            return false;
        }
    }

    return true;
}

bool fat32_write_file(const char* name, const void* data, uint32_t size) {
    if (!is_initialized || !data) return false;

    // First, create the file entry
    uint32_t current_cluster = current_directory.cluster;
    uint8_t dir_buffer[512];
    fat32_dir_entry_t* entry = NULL;
    uint32_t entry_sector = 0;
    uint32_t entry_offset = 0;
    bool found_slot = false;

    // Find a free directory entry
    while (current_cluster < 0x0FFFFFF8 && !found_slot) {
        uint32_t current_sector = cluster_to_lba(current_cluster);

        for (uint32_t i = 0; i < sectors_per_cluster && !found_slot; i++) {
            if (!ata_read_sectors(current_sector + i, 1, dir_buffer)) {
                return false;
            }

            entry = (fat32_dir_entry_t*)dir_buffer;
            for (uint32_t j = 0; j < 16; j++) {
                if (entry[j].name[0] == 0x00 || entry[j].name[0] == 0xE5) {
                    entry = &entry[j];
                    entry_sector = current_sector + i;
                    entry_offset = j;
                    found_slot = true;
                    break;
                }
            }
        }
        if (!found_slot) {
            current_cluster = fat32_get_next_cluster(current_cluster);
        }
    }

    if (!found_slot) return false;

    // Allocate first cluster for file data
    uint32_t first_cluster = fat32_allocate_cluster();
    if (!first_cluster) return false;

    // Create file entry
    memset(entry, 0, sizeof(fat32_dir_entry_t));
    memcpy(entry->name, name, 11);
    entry->attributes = 0x20;  // Archive bit
    entry->first_cluster_high = (uint16_t)(first_cluster >> 16);
    entry->first_cluster_low = (uint16_t)(first_cluster & 0xFFFF);
    entry->file_size = size;

    // Write directory entry
    if (!ata_write_sectors(entry_sector, 1, dir_buffer)) {
        return false;
    }

    // Write file data
    uint32_t bytes_written = 0;
    uint32_t current_data_cluster = first_cluster;

    while (bytes_written < size) {
        uint32_t data_sector = cluster_to_lba(current_data_cluster);
        uint32_t bytes_to_write = size - bytes_written;
        uint32_t sectors_to_write = (bytes_to_write + 511) / 512;

        if (sectors_to_write > sectors_per_cluster) {
            sectors_to_write = sectors_per_cluster;
        }

        // Write data
        if (!ata_write_sectors(data_sector, sectors_to_write, (uint8_t*)data + bytes_written)) {
            return false;
        }

        bytes_written += sectors_to_write * 512;

        // Allocate next cluster if needed
        if (bytes_written < size) {
            uint32_t next_cluster = fat32_allocate_cluster();
            if (!next_cluster) return false;

            if (!fat32_write_fat_entry(current_data_cluster, next_cluster)) {
                return false;
            }
            current_data_cluster = next_cluster;
        }
    }

    return true;
}

bool fat32_read_file(const char* name, void* buffer, uint32_t* size) {
    if (!is_initialized || !buffer || !size) return false;

    uint32_t current_cluster = current_directory.cluster;
    uint8_t dir_buffer[512];
    fat32_dir_entry_t* entry = NULL;
    bool found = false;

    // Find the file entry
    while (current_cluster < 0x0FFFFFF8 && !found) {
        uint32_t current_sector = cluster_to_lba(current_cluster);

        for (uint32_t i = 0; i < sectors_per_cluster && !found; i++) {
            if (!ata_read_sectors(current_sector + i, 1, dir_buffer)) {
                return false;
            }

            entry = (fat32_dir_entry_t*)dir_buffer;
            for (uint32_t j = 0; j < 16; j++) {
                if (entry[j].name[0] != 0x00 && entry[j].name[0] != 0xE5) {
                    // Use case-insensitive comparison
                    if (compare_filename(name, entry[j].name)) {
                        entry = &entry[j];
                        found = true;
                        break;
                    }
                }
            }
        }
        if (!found) {
            current_cluster = fat32_get_next_cluster(current_cluster);
        }
    }

    if (!found) return false;

    // Check buffer size
    if (*size < entry->file_size) {
        *size = entry->file_size;
        return false;
    }

    // Read file data
    *size = entry->file_size;
    uint32_t bytes_read = 0;
    uint32_t current_data_cluster = (uint32_t)entry->first_cluster_high << 16 | entry->first_cluster_low;

    while (bytes_read < entry->file_size && current_data_cluster < 0x0FFFFFF8) {
        uint32_t data_sector = cluster_to_lba(current_data_cluster);
        uint32_t bytes_to_read = entry->file_size - bytes_read;
        uint32_t sectors_to_read = (bytes_to_read + 511) / 512;

        if (sectors_to_read > sectors_per_cluster) {
            sectors_to_read = sectors_per_cluster;
        }

        // Read data
        if (!ata_read_sectors(data_sector, sectors_to_read, (uint8_t*)buffer + bytes_read)) {
            return false;
        }

        bytes_read += sectors_to_read * 512;
        current_data_cluster = fat32_get_next_cluster(current_data_cluster);
    }

    return true;
}