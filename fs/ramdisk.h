#ifndef RAMDISK_H
#define RAMDISK_H

#include "block.h"

/**
 * RAM Disk Block Device
 * 
 * A volatile block device backed by PMM-allocated physical pages.
 * Data lives in RAM only — lost on reboot.
 * 
 * Used as the initial filesystem backend. Can be swapped for ATA
 * later without changing any filesystem code.
 */

/**
 * ramdisk_init - Create a RAM disk of the specified size
 * @size_kb: Size in kilobytes (rounded up to page boundary)
 * 
 * Allocates physical pages from the PMM and returns a block_device_t
 * that reads/writes to that memory region.
 * 
 * Returns: Pointer to static block_device_t, or NULL on failure
 */
block_device_t* ramdisk_init(uint32_t size_kb);

#endif /* RAMDISK_H */
