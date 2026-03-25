/**
 * ramdisk.c - RAM Disk Block Device
 * 
 * Implements the block_device_t interface using PMM-allocated pages.
 * All reads/writes are memcpy operations to a contiguous memory buffer.
 */

#include "ramdisk.h"
#include "../kernel/pmm.h"
#include "../libc/string.h"
#include "../drivers/screen.h"

/* Static instance — one RAM disk at a time */
static block_device_t ramdisk_dev;
static uint8_t* ramdisk_base = 0;
static uint32_t ramdisk_size = 0;

/**
 * ramdisk_read - Read a block from RAM disk
 */
static int ramdisk_read(uint32_t block, void* buf) {
    if (!ramdisk_base) return -1;

    uint32_t offset = block * BLOCK_SIZE;
    if (offset + BLOCK_SIZE > ramdisk_size) return -1;

    memcpy(buf, ramdisk_base + offset, BLOCK_SIZE);
    return 0;
}

/**
 * ramdisk_write - Write a block to RAM disk
 */
static int ramdisk_write(uint32_t block, const void* buf) {
    if (!ramdisk_base) return -1;

    uint32_t offset = block * BLOCK_SIZE;
    if (offset + BLOCK_SIZE > ramdisk_size) return -1;

    memcpy(ramdisk_base + offset, buf, BLOCK_SIZE);
    return 0;
}

block_device_t* ramdisk_init(uint32_t size_kb) {
    /* Calculate pages needed (round up) */
    uint32_t size_bytes = size_kb * 1024;
    uint32_t pages_needed = (size_bytes + PAGE_SIZE - 1) / PAGE_SIZE;

    /* Allocate contiguous pages from PMM */
    uint32_t phys_addr = alloc_pages(pages_needed);
    if (phys_addr == 0) {
        kprint("ERROR: ramdisk_init() - Failed to allocate ");
        kprint_uint(pages_needed);
        kprint(" pages\n");
        return (block_device_t*)0;
    }

    /* Zero the entire region */
    ramdisk_base = (uint8_t*)phys_addr;
    ramdisk_size = pages_needed * PAGE_SIZE;
    memset(ramdisk_base, 0, ramdisk_size);

    /* Set up block device */
    ramdisk_dev.read = ramdisk_read;
    ramdisk_dev.write = ramdisk_write;
    ramdisk_dev.block_size = BLOCK_SIZE;
    ramdisk_dev.total_blocks = ramdisk_size / BLOCK_SIZE;

    return &ramdisk_dev;
}
