/*
 * FAT32 Rename Helper Implementation
 * Add this function to drivers/fat32.c or include it separately
 *
 * This implements file/directory renaming by updating the FAT32 directory entry
 */

#include "../include/fat32.h"
#include "../include/ata.h"
#include "../include/string.h"
#include "../include/vga.h"

/*
 * Reference-only helper:
 * This code uses fat32.c internal statics and is meant to be copied into
 * drivers/fat32.c, not compiled as a separate translation unit.
 */
#if 0

/* Rename a file or directory entry in FAT32
 *
 * Parameters:
 *   old_name - Current 11-byte FAT32 name (e.g., "HELLO  TXT")
 *   new_name - New 11-byte FAT32 name (e.g., "WORLD  TXT")
 *
 * Returns:
 *   true if rename successful, false otherwise
 *
 * This function:
 * 1. Searches the current directory for the file with old_name
 * 2. Finds the matching FAT32 directory entry
 * 3. Updates the filename in-place
 * 4. Writes the modified sector back to disk
 */
bool fat32_rename_file(const char* old_name, const char* new_name) {
    if (!is_initialized || !old_name || !new_name) {
        return false;
    }

    uint32_t current_cluster = current_directory.cluster;
    uint8_t buffer[512];
    fat32_dir_entry_t* entry;

    /* Search through all clusters in current directory */
    while (current_cluster < 0x0FFFFFF8) {
        uint32_t current_sector = cluster_to_lba(current_cluster);

        /* Read all sectors in this cluster */
        for (uint32_t i = 0; i < sectors_per_cluster; i++) {
            if (!ata_read_sectors(current_sector + i, 1, buffer)) {
                return false;
            }

            /* Search directory entries in this sector */
            entry = (fat32_dir_entry_t*)buffer;
            for (uint32_t j = 0; j < (SECTOR_SIZE / sizeof(fat32_dir_entry_t)); j++) {
                /* End of directory */
                if (entry[j].name[0] == 0x00) {
                    return false;
                }

                /* Skip deleted/special entries */
                if (entry[j].name[0] == 0xE5 ||
                    entry[j].attributes == 0x08 ||
                    entry[j].name[0] == '.') {
                    continue;
                }

                /* Check if this is the file we're looking for */
                bool matches = true;
                for (int k = 0; k < 11; k++) {
                    if (entry[j].name[k] != old_name[k]) {
                        matches = false;
                        break;
                    }
                }

                if (matches) {
                    /* Found it! Update the name */
                    for (int k = 0; k < 11; k++) {
                        entry[j].name[k] = new_name[k];
                    }

                    /* Write the modified sector back */
                    if (!ata_write_sectors(current_sector + i, 1, buffer)) {
                        return false;
                    }

                    return true;
                }
            }
        }

        /* Move to next cluster */
        current_cluster = fat32_get_next_cluster(current_cluster);
        if (current_cluster == 0) {
            return false;
        }
    }

    return false;
}

/* Alternative: faster rename using directory helper (if available) */
/* Uncomment if you have fat32_dir_entry_t integration: */
/*
bool fat32_rename_file_fast(const char* old_name, const char* new_name) {
    fat32_dir_entry_t entry;

    // Find the file
    if (!fat32_find_file(old_name, &entry)) {
        return false;
    }

    // Update the name in-place and rewrite
    // This would require a helper function that writes back a found entry
    // fat32_update_directory_entry(&entry, new_name);

    return true;
}
*/

#endif
