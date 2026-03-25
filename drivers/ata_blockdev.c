/**
 * ata_blockdev.c - ATA Block Device Adapter
 *
 * Implements the block_device_t interface by delegating to the ATA PIO
 * driver's slave read/write functions.  The primary master holds the
 * boot image; the primary slave holds the persistent filesystem.
 */

#include "ata_blockdev.h"
#include "ata.h"

static block_device_t ata_dev;

static int ata_block_read(uint32_t block, void* buf) {
    return ata_slave_read_sectors(block, 1, buf);
}

static int ata_block_write(uint32_t block, const void* buf) {
    return ata_slave_write_sectors(block, 1, buf);
}

block_device_t* ata_blockdev_init(void) {
    const ata_drive_info_t* info = ata_get_slave_info();
    if (!info || !info->present) return (block_device_t*)0;

    ata_dev.read  = ata_block_read;
    ata_dev.write = ata_block_write;
    ata_dev.block_size   = ATA_SECTOR_SIZE;
    ata_dev.total_blocks = info->lba28_sectors;

    return &ata_dev;
}
