#ifndef ATA_BLOCKDEV_H
#define ATA_BLOCKDEV_H

#include "../fs/block.h"

/**
 * ATA Block Device Adapter
 *
 * A thin adapter that implements the block_device_t interface over
 * the ATA PIO driver (primary slave).  The boot image occupies the
 * primary master, so the filesystem lives on the primary slave disk
 * (QEMU flag: -hdb bin/disk.img).
 *
 * Returns NULL if no slave drive is detected — caller should fall
 * back to the RAM disk.
 */

/**
 * ata_blockdev_init - Create a block_device_t backed by ATA primary slave
 *
 * Returns: Pointer to a static block_device_t, or NULL if no slave disk.
 */
block_device_t* ata_blockdev_init(void);

#endif /* ATA_BLOCKDEV_H */
