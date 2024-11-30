#ifndef RINGOS_ATA_H
#define RINGOS_ATA_H

#include "types.h"

// ATA registers
#define ATA_DATA        0x1F0   // Read/Write PIO data bytes
#define ATA_ERROR       0x1F1   // Read error register
#define ATA_FEATURES    0x1F1   // Write features
#define ATA_SECCOUNT0   0x1F2   // Number of sectors to read/write
#define ATA_SECTOR      0x1F3   // Sector address LBA 0-7
#define ATA_LCYL        0x1F4   // Cylinder low / LBA 8-15
#define ATA_HCYL        0x1F5   // Cylinder high / LBA 16-23
#define ATA_DRIVEHEAD   0x1F6   // Drive/Head / LBA 24-27
#define ATA_STATUS      0x1F7   // Read status
#define ATA_COMMAND     0x1F7   // Write command

// Status register bits
#define ATA_SR_BSY      0x80    // Busy
#define ATA_SR_DRDY     0x40    // Drive ready
#define ATA_SR_DF       0x20    // Drive write fault
#define ATA_SR_DSC      0x10    // Drive seek complete
#define ATA_SR_DRQ      0x08    // Data request ready
#define ATA_SR_CORR     0x04    // Corrected data
#define ATA_SR_IDX      0x02    // Index
#define ATA_SR_ERR      0x01    // Error

// Commands
#define ATA_CMD_READ_PIO          0x20
#define ATA_CMD_READ_PIO_EXT      0x24
#define ATA_CMD_READ_DMA          0xC8
#define ATA_CMD_READ_DMA_EXT      0x25
#define ATA_CMD_WRITE_PIO         0x30
#define ATA_CMD_WRITE_PIO_EXT     0x34
#define ATA_CMD_WRITE_DMA         0xCA
#define ATA_CMD_WRITE_DMA_EXT     0x35
#define ATA_CMD_CACHE_FLUSH       0xE7
#define ATA_CMD_CACHE_FLUSH_EXT   0xEA
#define ATA_CMD_PACKET            0xA0
#define ATA_CMD_IDENTIFY_PACKET   0xA1
#define ATA_CMD_IDENTIFY          0xEC

// IDENTIFY command response offsets
#define ATA_IDENT_DEVICETYPE   0
#define ATA_IDENT_CYLINDERS    2
#define ATA_IDENT_HEADS        6
#define ATA_IDENT_SECTORS      12
#define ATA_IDENT_SERIAL       20
#define ATA_IDENT_MODEL        54
#define ATA_IDENT_CAPABILITIES 98
#define ATA_IDENT_FIELDVALID   106
#define ATA_IDENT_MAX_LBA      120
#define ATA_IDENT_COMMANDSETS  164
#define ATA_IDENT_MAX_LBA_EXT  200

// Function prototypes
bool ata_init(void);
bool ata_identify(void);
bool ata_read_sectors(uint32_t lba, uint8_t sector_count, void* buffer);
bool ata_write_sectors(uint32_t lba, uint8_t sector_count, const void* buffer);

#endif /* RINGOS_ATA_H */