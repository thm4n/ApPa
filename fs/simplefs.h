#ifndef SIMPLEFS_H
#define SIMPLEFS_H

#include "block.h"
#include "../libc/stdint.h"

/**
 * SimpleFS - A minimal filesystem for ApPa OS
 * 
 * Disk layout (512-byte blocks):
 *   Block 0:       Superblock (magic, metadata, free block tracking)
 *   Block 1-N:     Directory entries (16 entries per block, 32 bytes each)
 *   Block N+1-end: Data blocks (file contents, contiguous allocation)
 * 
 * Simplifications (v1):
 *   - Flat directory (no subdirectories as nested storage yet)
 *   - Contiguous file allocation (no fragmentation handling)
 *   - Fixed directory size (2 blocks = 32 entries max)
 *   - Max filename: 23 characters
 */

/* ========== Constants ========== */
#define FS_MAGIC            0x41504653  /* "APFS" in little-endian */
#define FS_VERSION          1
#define FS_NAME_MAX         23          /* Max filename length (24 bytes - null) */
#define FS_DIR_BLOCKS       2           /* Blocks reserved for directory */
#define FS_ENTRIES_PER_BLOCK 16         /* 512 / 32 = 16 entries per block */
#define FS_MAX_ENTRIES      (FS_DIR_BLOCKS * FS_ENTRIES_PER_BLOCK)  /* 32 max */

/* ========== Entry Types ========== */
#define FS_TYPE_FREE        0x00
#define FS_TYPE_FILE        0x01
#define FS_TYPE_DIR         0x02

/* ========== Superblock (Block 0) ========== */
typedef struct {
    uint32_t magic;             /* FS_MAGIC = 0x41504653 */
    uint32_t version;           /* FS_VERSION = 1 */
    uint32_t total_blocks;      /* Total blocks on device */
    uint32_t free_blocks;       /* Number of free data blocks */
    uint32_t dir_start_block;   /* First directory block (always 1) */
    uint32_t dir_block_count;   /* Number of directory blocks (always 2) */
    uint32_t data_start_block;  /* First data block (always 3) */
    uint32_t first_free_block;  /* Hint: first free data block */
    uint8_t  reserved[480];     /* Pad to 512 bytes */
} __attribute__((packed)) fs_superblock_t;

/* ========== Directory Entry (32 bytes) ========== */
typedef struct {
    char     name[24];          /* Null-terminated filename (max 23 chars) */
    uint8_t  type;              /* FS_TYPE_FREE / FS_TYPE_FILE / FS_TYPE_DIR */
    uint8_t  reserved;
    uint16_t start_block;       /* First data block (relative to data region) */
    uint32_t size;              /* File size in bytes (0 for directories) */
} __attribute__((packed)) fs_entry_t;

/* ========== API ========== */

/**
 * fs_init - Initialize the filesystem on a block device
 * @dev: Block device to use
 * 
 * If the device has a valid superblock (correct magic), mounts it.
 * Otherwise, formats the device with a fresh SimpleFS.
 * 
 * Returns: 0 on success, -1 on error
 */
int fs_init(block_device_t* dev);

/**
 * fs_create - Create a new file or directory entry
 * @name: Entry name (max 23 chars)
 * @type: FS_TYPE_FILE or FS_TYPE_DIR
 * 
 * Returns: 0 on success, -1 on error (full, duplicate, bad name)
 */
int fs_create(const char* name, uint8_t type);

/**
 * fs_write_file - Write data to an existing file (overwrites)
 * @name:  File name
 * @data:  Data buffer
 * @size:  Number of bytes to write
 * 
 * Returns: 0 on success, -1 on error
 */
int fs_write_file(const char* name, const void* data, uint32_t size);

/**
 * fs_read_file - Read data from a file
 * @name:     File name
 * @buf:      Destination buffer
 * @max_size: Maximum bytes to read
 * 
 * Returns: Bytes read (>= 0), or -1 on error
 */
int32_t fs_read_file(const char* name, void* buf, uint32_t max_size);

/**
 * fs_delete - Delete a file or directory
 * @name: Entry name
 * 
 * Returns: 0 on success, -1 on error (not found)
 */
int fs_delete(const char* name);

/**
 * fs_list - List all entries in the root directory
 * @entries: Output array to fill
 * @max:     Maximum entries to return
 * 
 * Returns: Number of entries found
 */
uint32_t fs_list(fs_entry_t* entries, uint32_t max);

/**
 * fs_stat - Get info about a named entry
 * @name: Entry name
 * @out:  Output entry struct
 * 
 * Returns: 0 on success, -1 if not found
 */
int fs_stat(const char* name, fs_entry_t* out);

#endif /* SIMPLEFS_H */
