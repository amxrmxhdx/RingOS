#include "../include/ata.h"
#include "../include/io.h"
#include "../include/vga.h"

static bool ata_initialized = false;

static void ata_400ns_delay(void) {
    inb(ATA_STATUS);
    inb(ATA_STATUS);
    inb(ATA_STATUS);
    inb(ATA_STATUS);
}

static bool ata_wait_not_busy(void) {
    uint32_t timeout = 100000;
    while (--timeout) {
        uint8_t status = inb(ATA_STATUS);
        if (!(status & ATA_SR_BSY)) {
            return true;
        }
        ata_400ns_delay();
    }
    return false;
}

static bool ata_wait_drq(void) {
    uint32_t timeout = 100000;
    while (--timeout) {
        uint8_t status = inb(ATA_STATUS);
        if (status & ATA_SR_DRQ) {
            return true;
        }
        ata_400ns_delay();
    }
    return false;
}

bool ata_init(void) {
    if (ata_initialized) {
        return true;
    }

    // Select drive 0
    outb(ATA_DRIVEHEAD, 0xA0);
    ata_400ns_delay();

    // Check if drive exists
    if (!ata_wait_not_busy()) {
        vga_writestr("ATA Error: Drive busy timeout\n");
        return false;
    }

    outb(ATA_COMMAND, ATA_CMD_IDENTIFY);
    ata_400ns_delay();

    if (!ata_wait_drq()) {
        vga_writestr("ATA Error: Drive not ready\n");
        return false;
    }

    // Read and discard identify data
    for (int i = 0; i < 256; i++) {
        inw(ATA_DATA);
    }

    ata_initialized = true;
    return true;
}

bool ata_read_sectors(uint32_t lba, uint8_t sector_count, void* buffer) {
    if (!ata_initialized) {
        vga_writestr("ATA Error: Drive not initialized\n");
        return false;
    }

    if (!buffer) {
        vga_writestr("ATA Error: Invalid buffer\n");
        return false;
    }

    // Wait for drive ready
    if (!ata_wait_not_busy()) {
        vga_writestr("ATA Error: Drive busy before read\n");
        return false;
    }

    // Send drive select and LBA
    outb(ATA_DRIVEHEAD, 0xE0 | ((lba >> 24) & 0x0F));
    outb(ATA_SECCOUNT0, sector_count);
    outb(ATA_SECTOR, (uint8_t)lba);
    outb(ATA_LCYL, (uint8_t)(lba >> 8));
    outb(ATA_HCYL, (uint8_t)(lba >> 16));

    // Send read command
    outb(ATA_COMMAND, ATA_CMD_READ_PIO);

    uint16_t* buf = (uint16_t*)buffer;
    for (int s = 0; s < sector_count; s++) {
        if (!ata_wait_drq()) {
            vga_writestr("ATA Error: Data not ready\n");
            return false;
        }

        // Read sector
        for (int i = 0; i < 256; i++) {
            buf[i + (s * 256)] = inw(ATA_DATA);
        }
    }

    return true;
}

bool ata_write_sectors(uint32_t lba, uint8_t sector_count, const void* buffer) {
    if (!ata_initialized) {
        vga_writestr("ATA Error: Drive not initialized\n");
        return false;
    }

    if (!buffer) {
        vga_writestr("ATA Error: Invalid buffer\n");
        return false;
    }

    // Wait for drive ready
    if (!ata_wait_not_busy()) {
        vga_writestr("ATA Error: Drive busy before write\n");
        return false;
    }

    // Send drive select and LBA
    outb(ATA_DRIVEHEAD, 0xE0 | ((lba >> 24) & 0x0F));
    outb(ATA_SECCOUNT0, sector_count);
    outb(ATA_SECTOR, (uint8_t)lba);
    outb(ATA_LCYL, (uint8_t)(lba >> 8));
    outb(ATA_HCYL, (uint8_t)(lba >> 16));

    // Send write command
    outb(ATA_COMMAND, ATA_CMD_WRITE_PIO);

    const uint16_t* buf = (const uint16_t*)buffer;
    for (int s = 0; s < sector_count; s++) {
        if (!ata_wait_drq()) {
            vga_writestr("ATA Error: Drive not ready for write\n");
            return false;
        }

        // Write sector
        for (int i = 0; i < 256; i++) {
            outw(ATA_DATA, buf[i + (s * 256)]);
        }

        // Flush write cache
        outb(ATA_COMMAND, ATA_CMD_CACHE_FLUSH);
        if (!ata_wait_not_busy()) {
            vga_writestr("ATA Error: Cache flush failed\n");
            return false;
        }
    }

    return true;
}