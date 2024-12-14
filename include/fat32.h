#ifndef RINGOS_FAT32_H
#define RINGOS_FAT32_H

#include "types.h"

// FAT32 specific constants
#define SECTOR_SIZE 512
#define FAT_ENTRY_SIZE 4
#define DIR_ENTRY_SIZE 32

// FAT32 file attributes
#define ATTR_READ_ONLY 0x01
#define ATTR_HIDDEN    0x02
#define ATTR_SYSTEM    0x04
#define ATTR_VOLUME_ID 0x08
#define ATTR_DIRECTORY 0x10
#define ATTR_ARCHIVE   0x20
#define ATTR_LONG_NAME (ATTR_READ_ONLY | ATTR_HIDDEN | ATTR_SYSTEM | ATTR_VOLUME_ID)

// FAT32 structures
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
    uint32_t cluster;
    char name[12];
    char path[256];
} directory_entry_t;

// Function prototypes
bool fat32_init(void);
bool fat32_read_boot_sector(fat32_boot_sector_t* boot_sector);
bool fat32_read_root_directory(void);
bool fat32_find_file(const char* name, fat32_dir_entry_t* entry);
uint32_t fat32_get_next_cluster(uint32_t current_cluster);
bool fat32_create_file(const char* name);
bool fat32_delete_file(const char* name);
bool fat32_list_directory(void (*callback)(const char* name, uint32_t size, uint8_t attr));
bool fat32_create_directory(const char* name);
bool fat32_init_directory_structure(void);
bool fat32_write_file(const char* name, const void* data, uint32_t size);
bool fat32_read_file(const char* name, void* buffer, uint32_t* size);
bool fat32_free_clusters(uint32_t first_cluster);
uint32_t fat32_allocate_cluster(void);
bool fat32_write_fat_entry(uint32_t cluster, uint32_t value);
bool fat32_change_directory(const char* dirname);
uint32_t fat32_get_current_directory(void);
const char* fat32_get_current_path(void);
bool fat32_is_directory(const fat32_dir_entry_t* entry);

// Add these helper macros
#define FAT32_EOC 0x0FFFFFF8  // End of chain marker
#define FAT32_FREE_CLUSTER 0x00000000

#endif /* RINGOS_FAT32_H */
