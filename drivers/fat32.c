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

    is_initialized = true;
    return true;
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

bool fat32_list_directory(void (*callback)(const char* name, uint32_t size, uint8_t attr)) {
    if (!is_initialized) return false;

    uint32_t current_cluster = boot_sector.root_cluster;
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
                if (entry[j].name[0] == 0x00) {
                    return true;
                }
                if (entry[j].name[0] == 0xE5) {
                    continue;
                }
                if ((entry[j].attributes & 0x0F) == 0) {
                    callback((char*)entry[j].name, entry[j].file_size, entry[j].attributes);
                }
            }
        }

        current_cluster = fat32_get_next_cluster(current_cluster);
    }

    return true;
}

bool fat32_create_file(const char* name) {
    if (!is_initialized) return false;

    uint32_t current_cluster = boot_sector.root_cluster;
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

    uint32_t current_cluster = boot_sector.root_cluster;
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