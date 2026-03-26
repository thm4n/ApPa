#ifndef BLOCK_H
#define BLOCK_H

#include "../klibc/stdint.h"

/**
 * Block Device Interface
 * 
 * Abstract interface for any block-addressable storage device.
 * Filesystem code talks to this interface, not directly to hardware.
 * 
 * Implementations:
 *   - RAM disk  (fs/ramdisk.c)  — PMM-backed memory, fast, volatile
 *   - ATA disk  (drivers/ata.c) — real hardware, persistent (future)
 */

#define BLOCK_SIZE 512  /* Standard sector size, matches ATA */

typedef struct {
    /**
     * read - Read a single block from the device
     * @block: Block number (0-indexed)
     * @buf:   Destination buffer (must be >= BLOCK_SIZE bytes)
     * Returns: 0 on success, -1 on error
     */
    int (*read)(uint32_t block, void* buf);

    /**
     * write - Write a single block to the device
     * @block: Block number (0-indexed)
     * @buf:   Source buffer (must be >= BLOCK_SIZE bytes)
     * Returns: 0 on success, -1 on error
     */
    int (*write)(uint32_t block, const void* buf);

    uint32_t block_size;    /* Bytes per block (always 512 for now) */
    uint32_t total_blocks;  /* Total number of blocks on device */
} block_device_t;

#endif /* BLOCK_H */
