#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <dirent.h>
#include <sys/stat.h>
#include <errno.h>
#include <time.h>

#define SECTOR_SIZE 512
#define FILESYSTEM_SIZE (32 * 1024 * 1024)  // 32MB filesystem
#define RESERVED_SECTORS 32
#define FAT_COPIES 2
#define SECTORS_PER_CLUSTER 8
#define MAX_PATH_LENGTH 1024

typedef struct {
    uint8_t  jump_boot[3];
    uint8_t  oem_name[8];
    uint16_t bytes_per_sector;
    uint8_t  sectors_per_cluster;
    uint16_t reserved_sectors;
    uint8_t  num_fats;
    uint16_t root_entry_count;
    uint16_t total_sectors_16;
    uint8_t  media_type;
    uint16_t fat_size_16;
    uint16_t sectors_per_track;
    uint16_t num_heads;
    uint32_t hidden_sectors;
    uint32_t total_sectors_32;
    uint32_t fat_size_32;
    uint16_t ext_flags;
    uint16_t fs_version;
    uint32_t root_cluster;
    uint16_t fs_info;
    uint16_t backup_boot_sector;
    uint8_t  reserved[12];
    uint8_t  drive_number;
    uint8_t  reserved1;
    uint8_t  boot_signature;
    uint32_t volume_id;
    uint8_t  volume_label[11];
    uint8_t  fs_type[8];
} __attribute__((packed)) fat32_boot_sector_t;

typedef struct {
    uint8_t  name[11];
    uint8_t  attributes;
    uint8_t  nt_reserved;
    uint8_t  creation_time_tenths;
    uint16_t creation_time;
    uint16_t creation_date;
    uint16_t last_access_date;
    uint16_t first_cluster_high;
    uint16_t last_write_time;
    uint16_t last_write_date;
    uint16_t first_cluster_low;
    uint32_t file_size;
} __attribute__((packed)) fat32_dir_entry_t;

typedef struct {
    unsigned char* buffer;
    size_t size;
    uint32_t fat_start;
    uint32_t cluster_start;
    uint32_t next_free_cluster;
    uint32_t data_size;
} filesystem_image;

static void initialize_boot_sector(fat32_boot_sector_t* bs) {
    memset(bs, 0, sizeof(fat32_boot_sector_t));
    
    bs->jump_boot[0] = 0xEB;
    bs->jump_boot[1] = 0x58;
    bs->jump_boot[2] = 0x90;
    
    memcpy(bs->oem_name, "MSWIN4.1", 8);
    bs->bytes_per_sector = SECTOR_SIZE;
    bs->sectors_per_cluster = SECTORS_PER_CLUSTER;
    bs->reserved_sectors = RESERVED_SECTORS;
    bs->num_fats = FAT_COPIES;
    bs->media_type = 0xF8;
    bs->sectors_per_track = 32;
    bs->num_heads = 64;
    bs->hidden_sectors = 0;
    bs->total_sectors_32 = FILESYSTEM_SIZE / SECTOR_SIZE;
    
    uint32_t data_sectors = bs->total_sectors_32 - bs->reserved_sectors;
    uint32_t cluster_count = data_sectors / bs->sectors_per_cluster;
    bs->fat_size_32 = ((cluster_count * 4) + (SECTOR_SIZE - 1)) / SECTOR_SIZE;
    
    bs->ext_flags = 0;
    bs->fs_version = 0;
    bs->root_cluster = 2;
    bs->fs_info = 1;
    bs->backup_boot_sector = 6;
    bs->drive_number = 0x80;
    bs->boot_signature = 0x29;
    bs->volume_id = 0x12345678;
    memcpy(bs->volume_label, "NO NAME    ", 11);
    memcpy(bs->fs_type, "FAT32   ", 8);
}

static filesystem_image* create_filesystem(void) {
    filesystem_image* fs = malloc(sizeof(filesystem_image));
    if (!fs) {
        fprintf(stderr, "Failed to allocate filesystem structure\n");
        return NULL;
    }

    fs->buffer = calloc(1, FILESYSTEM_SIZE);
    if (!fs->buffer) {
        fprintf(stderr, "Failed to allocate filesystem buffer\n");
        free(fs);
        return NULL;
    }

    fat32_boot_sector_t* bs = (fat32_boot_sector_t*)fs->buffer;
    initialize_boot_sector(bs);

    fs->size = FILESYSTEM_SIZE;
    fs->fat_start = bs->reserved_sectors * SECTOR_SIZE;
    fs->cluster_start = (bs->reserved_sectors + (bs->num_fats * bs->fat_size_32)) * SECTOR_SIZE;
    fs->next_free_cluster = 2;  // Clusters 0 and 1 are reserved
    fs->data_size = FILESYSTEM_SIZE - fs->cluster_start;

    // Initialize FAT
    uint32_t* fat = (uint32_t*)(fs->buffer + fs->fat_start);
    fat[0] = 0x0FFFFFF8;  // Media descriptor
    fat[1] = 0x0FFFFFFF;  // EOC marker
    fat[2] = 0x0FFFFFFF;  // EOC for root directory

    // Copy FAT to second FAT
    memcpy(fs->buffer + fs->fat_start + (bs->fat_size_32 * SECTOR_SIZE),
           fs->buffer + fs->fat_start,
           bs->fat_size_32 * SECTOR_SIZE);

    return fs;
}

static void destroy_filesystem(filesystem_image* fs) {
    if (fs) {
        free(fs->buffer);
        free(fs);
    }
}

static uint32_t allocate_clusters(filesystem_image* fs, uint32_t count) {
    fat32_boot_sector_t* bs = (fat32_boot_sector_t*)fs->buffer;
    
    // Calculate total available clusters
    uint32_t total_clusters = (fs->size - fs->cluster_start) / (bs->sectors_per_cluster * SECTOR_SIZE);
    
    // Find free clusters
    uint32_t first_cluster = fs->next_free_cluster;
    uint32_t found_clusters = 0;
    uint32_t* fat = (uint32_t*)(fs->buffer + fs->fat_start);

    // Make sure we have enough space
    if (first_cluster + count >= total_clusters) {
        fprintf(stderr, "Error: Not enough space for %d clusters\n", count);
        return 0;
    }

    // Mark clusters as used
    for (uint32_t i = 0; i < count; i++) {
        uint32_t current_cluster = first_cluster + i;
        
        // Mark in primary FAT
        fat[current_cluster] = (i == count - 1) ? 0x0FFFFFFF : current_cluster + 1;
        
        // Mark in second FAT
        uint32_t* fat2 = (uint32_t*)(fs->buffer + fs->fat_start + (bs->fat_size_32 * SECTOR_SIZE));
        fat2[current_cluster] = fat[current_cluster];
    }

    // Update next free cluster
    fs->next_free_cluster = first_cluster + count;

    return first_cluster;
}

static void create_directory_entry(fat32_dir_entry_t* entry, const char* name, uint32_t cluster, 
                                 uint32_t size, uint8_t attr) {
    memset(entry, 0, sizeof(fat32_dir_entry_t));
    
    // Convert filename to 8.3 format
    memset(entry->name, ' ', 11);
    const char* dot = strchr(name, '.');
    size_t name_len = dot ? (dot - name) : strlen(name);
    if (name_len > 8) name_len = 8;
    memcpy(entry->name, name, name_len);
    
    if (dot && dot[1]) {
        memcpy(entry->name + 8, dot + 1, strlen(dot + 1) > 3 ? 3 : strlen(dot + 1));
    }

    entry->attributes = attr;
    entry->first_cluster_high = (uint16_t)(cluster >> 16);
    entry->first_cluster_low = (uint16_t)(cluster & 0xFFFF);
    entry->file_size = size;

    // Set creation time/date
    time_t t = time(NULL);
    struct tm* tm = localtime(&t);
    entry->creation_time = (tm->tm_hour << 11) | (tm->tm_min << 5) | (tm->tm_sec / 2);
    entry->creation_date = ((tm->tm_year - 80) << 9) | ((tm->tm_mon + 1) << 5) | tm->tm_mday;
    entry->last_write_time = entry->creation_time;
    entry->last_write_date = entry->creation_date;
}

static int process_file(filesystem_image* fs, const char* path, const char* name, uint32_t parent_cluster) {
    FILE* file = fopen(path, "rb");
    if (!file) {
        fprintf(stderr, "Error opening file %s: %s\n", path, strerror(errno));
        return -1;
    }

    fseek(file, 0, SEEK_END);
    uint32_t size = ftell(file);
    fseek(file, 0, SEEK_SET);

    uint32_t clusters_needed = (size + (SECTORS_PER_CLUSTER * SECTOR_SIZE) - 1) / (SECTORS_PER_CLUSTER * SECTOR_SIZE);
    if (clusters_needed == 0) clusters_needed = 1;  // Always allocate at least one cluster

    fprintf(stderr, "Processing file %s (size: %u bytes, needs %u clusters)\n", name, size, clusters_needed);

    uint32_t first_cluster = allocate_clusters(fs, clusters_needed);
    if (!first_cluster) {
        fprintf(stderr, "Failed to allocate %u clusters for %s\n", clusters_needed, name);
        fclose(file);
        return -1;
    }

    // Read file data
    uint8_t* cluster_ptr = fs->buffer + fs->cluster_start + 
                          ((first_cluster - 2) * SECTORS_PER_CLUSTER * SECTOR_SIZE);
    size_t bytes_read = fread(cluster_ptr, 1, size, file);
    if (bytes_read != size) {
        fprintf(stderr, "Failed to read file data for %s (read %zu of %u bytes)\n", 
                name, bytes_read, size);
        fclose(file);
        return -1;
    }

    // Create directory entry
    fat32_dir_entry_t* parent_dir = (fat32_dir_entry_t*)(fs->buffer + fs->cluster_start + 
                                   ((parent_cluster - 2) * SECTORS_PER_CLUSTER * SECTOR_SIZE));
    int entry_index = -1;
    for (int i = 0; i < (SECTORS_PER_CLUSTER * SECTOR_SIZE) / sizeof(fat32_dir_entry_t); i++) {
        if (parent_dir[i].name[0] == 0x00 || parent_dir[i].name[0] == 0xE5) {
            entry_index = i;
            break;
        }
    }

    if (entry_index == -1) {
        fprintf(stderr, "No free directory entries in parent directory\n");
        fclose(file);
        return -1;
    }

    memset(&parent_dir[entry_index], 0, sizeof(fat32_dir_entry_t));
    memcpy(parent_dir[entry_index].name, name, 11);
    parent_dir[entry_index].attributes = 0x20;  // Archive bit
    parent_dir[entry_index].first_cluster_high = (uint16_t)(first_cluster >> 16);
    parent_dir[entry_index].first_cluster_low = (uint16_t)(first_cluster & 0xFFFF);
    parent_dir[entry_index].file_size = size;

    fclose(file);
    return 0;
}

static int process_directory(filesystem_image* fs, const char* path, const char* name, uint32_t parent_cluster) {
    DIR* dir = opendir(path);
    if (!dir) {
        fprintf(stderr, "Error opening directory %s: %s\n", path, strerror(errno));
        return -1;
    }

    // Allocate cluster for directory
    uint32_t dir_cluster = allocate_clusters(fs, 1);
    if (!dir_cluster) {
        fprintf(stderr, "Failed to allocate cluster for directory %s\n", name);
        closedir(dir);
        return -1;
    }

    // Create directory entry in parent
    if (parent_cluster) {  // Skip for root directory
        fat32_dir_entry_t* parent_dir = (fat32_dir_entry_t*)(fs->buffer + fs->cluster_start + 
                                       ((parent_cluster - 2) * SECTORS_PER_CLUSTER * SECTOR_SIZE));
        int entry_index = -1;
        for (int i = 0; i < (SECTORS_PER_CLUSTER * SECTOR_SIZE) / sizeof(fat32_dir_entry_t); i++) {
            if (parent_dir[i].name[0] == 0x00 || parent_dir[i].name[0] == 0xE5) {
                entry_index = i;
                break;
            }
        }

        if (entry_index == -1) {
            fprintf(stderr, "No free directory entries in parent directory\n");
            closedir(dir);
            return -1;
        }

        create_directory_entry(&parent_dir[entry_index], name, dir_cluster, 0, 0x10);
    }

    // Create "." and ".." entries
    fat32_dir_entry_t* dir_entries = (fat32_dir_entry_t*)(fs->buffer + fs->cluster_start + 
                                    ((dir_cluster - 2) * SECTORS_PER_CLUSTER * SECTOR_SIZE));
    create_directory_entry(&dir_entries[0], ".", dir_cluster, 0, 0x10);
    create_directory_entry(&dir_entries[1], "..", parent_cluster ? parent_cluster : dir_cluster, 0, 0x10);

    struct dirent* entry;
    char full_path[MAX_PATH_LENGTH];
    int ret = 0;

    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_name[0] == '.') continue;

        if ((size_t)snprintf(full_path, sizeof(full_path), "%s/%s", path, entry->d_name) >= sizeof(full_path)) {
            fprintf(stderr, "Path too long: %s/%s\n", path, entry->d_name);
            ret = -1;
            break;
        }

        struct stat st;
        if (stat(full_path, &st) != 0) {
            fprintf(stderr, "Error stating %s: %s\n", full_path, strerror(errno));
            ret = -1;
            break;
        }

        if (S_ISDIR(st.st_mode)) {
            if (process_directory(fs, full_path, entry->d_name, dir_cluster) != 0) {
                ret = -1;
                break;
            }
        } else if (S_ISREG(st.st_mode)) {
            if (process_file(fs, full_path, entry->d_name, dir_cluster) != 0) {
                ret = -1;
                break;
            }
        }
    }

    closedir(dir);
    return ret;
}

static int create_filesystem_image(const char* source_dir, const char* output_file) {
    filesystem_image* fs = create_filesystem();
    if (!fs) return -1;

    // Process the root directory
    int ret = process_directory(fs, source_dir, "", 0);  // 0 indicates root directory

    if (ret == 0) {
        FILE* out = fopen(output_file, "wb");
        if (!out) {
            fprintf(stderr, "Error creating output file %s: %s\n", output_file, strerror(errno));
            ret = -1;
        } else {
            size_t written = fwrite(fs->buffer, 1, fs->size, out);
            if (written != fs->size) {
                fprintf(stderr, "Error writing to output file: %s\n", strerror(errno));
                ret = -1;
            }
            fclose(out);

            if (ret == 0) {
                printf("Filesystem image successfully created: %s\n", output_file);
            }
        }
    }

    destroy_filesystem(fs);
    return ret;
}

int main(int argc, char** argv) {
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <source_dir> <output_file>\n", argv[0]);
        return 1;
    }

    struct stat st;
    if (stat(argv[1], &st) != 0 || !S_ISDIR(st.st_mode)) {
        fprintf(stderr, "Error: %s is not a directory\n", argv[1]);
        return 1;
    }

    return create_filesystem_image(argv[1], argv[2]) == 0 ? 0 : 1;
}