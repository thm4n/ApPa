/**
 * simplefs.c - SimpleFS Implementation
 * 
 * A minimal flat-directory filesystem backed by a block device.
 * Superblock at block 0, directory at blocks 1-2, data from block 3 onward.
 */

#include "simplefs.h"
#include "../libc/string.h"
#include "../drivers/screen.h"

/* ========== State ========== */
static block_device_t* fs_dev = (block_device_t*)0;
static fs_superblock_t superblock;

/* Scratch buffer for block I/O (avoids stack allocation of 512 bytes) */
static uint8_t block_buf[BLOCK_SIZE];

/* ========== Internal Helpers ========== */

/**
 * fs_flush_superblock - Write superblock back to block 0
 */
static int fs_flush_superblock(void) {
    return fs_dev->write(0, &superblock);
}

/**
 * fs_read_entry - Read a directory entry by index
 * @index: Entry index (0 to FS_MAX_ENTRIES-1)
 * @entry: Output entry
 * Returns: 0 on success, -1 on error
 */
static int fs_read_entry(uint32_t index, fs_entry_t* entry) {
    if (index >= FS_MAX_ENTRIES) return -1;

    uint32_t block = superblock.dir_start_block + (index / FS_ENTRIES_PER_BLOCK);
    uint32_t offset = (index % FS_ENTRIES_PER_BLOCK) * sizeof(fs_entry_t);

    if (fs_dev->read(block, block_buf) != 0) return -1;

    memcpy(entry, block_buf + offset, sizeof(fs_entry_t));
    return 0;
}

/**
 * fs_write_entry - Write a directory entry by index
 * @index: Entry index (0 to FS_MAX_ENTRIES-1)
 * @entry: Entry to write
 * Returns: 0 on success, -1 on error
 */
static int fs_write_entry(uint32_t index, const fs_entry_t* entry) {
    if (index >= FS_MAX_ENTRIES) return -1;

    uint32_t block = superblock.dir_start_block + (index / FS_ENTRIES_PER_BLOCK);
    uint32_t offset = (index % FS_ENTRIES_PER_BLOCK) * sizeof(fs_entry_t);

    /* Read-modify-write the directory block */
    if (fs_dev->read(block, block_buf) != 0) return -1;
    memcpy(block_buf + offset, entry, sizeof(fs_entry_t));
    if (fs_dev->write(block, block_buf) != 0) return -1;

    return 0;
}

/**
 * fs_find_entry - Find a directory entry by name
 * @name:  Entry name to search for
 * @entry: Output entry (if found)
 * @index: Output index (if found, can be NULL)
 * Returns: 0 if found, -1 if not found
 */
static int fs_find_entry(const char* name, fs_entry_t* entry, uint32_t* index) {
    for (uint32_t i = 0; i < FS_MAX_ENTRIES; i++) {
        fs_entry_t e;
        if (fs_read_entry(i, &e) != 0) continue;
        if (e.type != FS_TYPE_FREE && strcmp(e.name, name) == 0) {
            if (entry) *entry = e;
            if (index) *index = i;
            return 0;
        }
    }
    return -1;
}

/**
 * fs_find_free_slot - Find a free directory entry slot
 * Returns: Index of free slot, or FS_MAX_ENTRIES if full
 */
static uint32_t fs_find_free_slot(void) {
    for (uint32_t i = 0; i < FS_MAX_ENTRIES; i++) {
        fs_entry_t e;
        if (fs_read_entry(i, &e) != 0) continue;
        if (e.type == FS_TYPE_FREE)
            return i;
    }
    return FS_MAX_ENTRIES;
}

/**
 * fs_alloc_blocks - Allocate contiguous data blocks
 * @count: Number of blocks needed
 * Returns: Starting data block number, or 0 on failure
 * 
 * Simple scan: find 'count' consecutive free blocks starting
 * from first_free_block hint. A block is "free" if it's not
 * referenced by any directory entry.
 * 
 * For v1, we use a linear scan of the directory to build an
 * occupancy map, then find a gap.
 */
static uint32_t fs_alloc_blocks(uint32_t count) {
    if (count == 0) return 0xFFFFFFFF;
    if (count > superblock.free_blocks) return 0xFFFFFFFF;

    uint32_t data_blocks = superblock.total_blocks - superblock.data_start_block;

    /* Build a simple used-block bitmap on the stack.
     * Max data blocks for a 256KB ramdisk: ~500 blocks = ~63 bytes bitmap.
     * Use a fixed-size array to avoid dynamic allocation. */
    uint8_t used[128];  /* Supports up to 1024 data blocks (512KB) */
    memset(used, 0, sizeof(used));

    /* Mark blocks used by existing files */
    for (uint32_t i = 0; i < FS_MAX_ENTRIES; i++) {
        fs_entry_t e;
        if (fs_read_entry(i, &e) != 0) continue;
        if (e.type == FS_TYPE_FREE) continue;
        if (e.type == FS_TYPE_DIR) continue;  /* Dirs don't use data blocks in v1 */

        uint32_t file_blocks = (e.size + BLOCK_SIZE - 1) / BLOCK_SIZE;
        for (uint32_t b = 0; b < file_blocks && (e.start_block + b) < data_blocks; b++) {
            uint32_t blk = e.start_block + b;
            if (blk / 8 < sizeof(used))
                used[blk / 8] |= (1 << (blk % 8));
        }
    }

    /* Find 'count' consecutive free blocks */
    uint32_t consecutive = 0;
    uint32_t start = 0;

    for (uint32_t b = 0; b < data_blocks && b / 8 < sizeof(used); b++) {
        if (!(used[b / 8] & (1 << (b % 8)))) {
            if (consecutive == 0) start = b;
            consecutive++;
            if (consecutive == count)
                return start;
        } else {
            consecutive = 0;
        }
    }

    return 0xFFFFFFFF;  /* Can't find contiguous block */
}

/**
 * fs_format - Format the device with a fresh SimpleFS
 */
static int fs_format(void) {
    /* Initialize superblock */
    memset(&superblock, 0, sizeof(superblock));
    superblock.magic = FS_MAGIC;
    superblock.version = FS_VERSION;
    superblock.total_blocks = fs_dev->total_blocks;
    superblock.dir_start_block = 1;
    superblock.dir_block_count = FS_DIR_BLOCKS;
    superblock.data_start_block = 1 + FS_DIR_BLOCKS;  /* Block 3 */
    superblock.first_free_block = 0;
    superblock.free_blocks = superblock.total_blocks - superblock.data_start_block;

    /* Write superblock */
    if (fs_flush_superblock() != 0) return -1;

    /* Zero directory blocks */
    memset(block_buf, 0, BLOCK_SIZE);
    for (uint32_t i = 0; i < FS_DIR_BLOCKS; i++) {
        if (fs_dev->write(superblock.dir_start_block + i, block_buf) != 0)
            return -1;
    }

    return 0;
}

/* ========== Public API ========== */

int fs_init(block_device_t* dev) {
    if (!dev) return -1;
    fs_dev = dev;

    /* Try to read existing superblock */
    if (fs_dev->read(0, &superblock) != 0) {
        /* Can't read block 0 — format */
        return fs_format();
    }

    /* Check magic */
    if (superblock.magic != FS_MAGIC) {
        /* No valid filesystem — format */
        return fs_format();
    }

    /* Valid filesystem found */
    return 0;
}

int fs_create(const char* name, uint8_t type) {
    if (!fs_dev || !name) return -1;
    if (strlen(name) == 0 || strlen(name) > FS_NAME_MAX) return -1;
    if (type != FS_TYPE_FILE && type != FS_TYPE_DIR) return -1;

    /* Check for duplicate */
    fs_entry_t existing;
    if (fs_find_entry(name, &existing, (uint32_t*)0) == 0) return -1;

    /* Find free slot */
    uint32_t slot = fs_find_free_slot();
    if (slot >= FS_MAX_ENTRIES) return -1;

    /* Create entry */
    fs_entry_t entry;
    memset(&entry, 0, sizeof(entry));
    strncpy(entry.name, name, FS_NAME_MAX);
    entry.name[FS_NAME_MAX] = '\0';
    entry.type = type;
    entry.start_block = 0;
    entry.size = 0;

    if (fs_write_entry(slot, &entry) != 0) return -1;

    return 0;
}

int fs_write_file(const char* name, const void* data, uint32_t size) {
    if (!fs_dev || !name || !data) return -1;

    /* Find the file */
    fs_entry_t entry;
    uint32_t index;
    if (fs_find_entry(name, &entry, &index) != 0) return -1;
    if (entry.type != FS_TYPE_FILE) return -1;

    /* Calculate blocks needed */
    uint32_t blocks_needed = (size + BLOCK_SIZE - 1) / BLOCK_SIZE;
    if (blocks_needed == 0) blocks_needed = 0;  /* Allow empty write */

    /* If file already has data, we could try to reuse blocks.
     * For simplicity in v1, we just allocate fresh blocks.
     * (Old blocks become implicitly free since nothing references them.) */

    uint32_t start_block = 0;
    if (blocks_needed > 0) {
        start_block = fs_alloc_blocks(blocks_needed);
        if (start_block == 0xFFFFFFFF) return -1;  /* No space */
    }

    /* Write data blocks */
    const uint8_t* src = (const uint8_t*)data;
    uint32_t remaining = size;

    for (uint32_t i = 0; i < blocks_needed; i++) {
        uint32_t abs_block = superblock.data_start_block + start_block + i;
        uint32_t chunk = remaining > BLOCK_SIZE ? BLOCK_SIZE : remaining;

        /* Handle partial last block — zero-fill */
        if (chunk < BLOCK_SIZE) {
            memset(block_buf, 0, BLOCK_SIZE);
            memcpy(block_buf, src, chunk);
            if (fs_dev->write(abs_block, block_buf) != 0) return -1;
        } else {
            if (fs_dev->write(abs_block, src) != 0) return -1;
        }

        src += chunk;
        remaining -= chunk;
    }

    /* Update directory entry */
    entry.start_block = (uint16_t)start_block;
    entry.size = size;
    if (fs_write_entry(index, &entry) != 0) return -1;

    /* Update superblock free count (approximate) */
    uint32_t old_blocks = 0;
    /* Recalculate free blocks from scratch for accuracy */
    uint32_t total_used = 0;
    for (uint32_t i = 0; i < FS_MAX_ENTRIES; i++) {
        fs_entry_t e;
        if (fs_read_entry(i, &e) != 0) continue;
        if (e.type == FS_TYPE_FILE && e.size > 0) {
            total_used += (e.size + BLOCK_SIZE - 1) / BLOCK_SIZE;
        }
    }
    uint32_t data_total = superblock.total_blocks - superblock.data_start_block;
    superblock.free_blocks = data_total > total_used ? data_total - total_used : 0;
    fs_flush_superblock();

    return 0;
}

int32_t fs_read_file(const char* name, void* buf, uint32_t max_size) {
    if (!fs_dev || !name || !buf) return -1;

    /* Find the file */
    fs_entry_t entry;
    if (fs_find_entry(name, &entry, (uint32_t*)0) != 0) return -1;
    if (entry.type != FS_TYPE_FILE) return -1;

    /* Calculate how much to read */
    uint32_t to_read = entry.size;
    if (to_read > max_size) to_read = max_size;

    uint32_t blocks = (to_read + BLOCK_SIZE - 1) / BLOCK_SIZE;
    uint8_t* dst = (uint8_t*)buf;
    uint32_t remaining = to_read;

    for (uint32_t i = 0; i < blocks; i++) {
        uint32_t abs_block = superblock.data_start_block + entry.start_block + i;
        if (fs_dev->read(abs_block, block_buf) != 0) return -1;

        uint32_t chunk = remaining > BLOCK_SIZE ? BLOCK_SIZE : remaining;
        memcpy(dst, block_buf, chunk);
        dst += chunk;
        remaining -= chunk;
    }

    return (int32_t)to_read;
}

int fs_delete(const char* name) {
    if (!fs_dev || !name) return -1;

    /* Find the entry */
    fs_entry_t entry;
    uint32_t index;
    if (fs_find_entry(name, &entry, &index) != 0) return -1;

    /* Calculate data blocks to free */
    uint32_t freed_blocks = 0;
    if (entry.type == FS_TYPE_FILE && entry.size > 0) {
        freed_blocks = (entry.size + BLOCK_SIZE - 1) / BLOCK_SIZE;
    }

    /* If deleting a directory, also remove all children whose names
     * start with "<dirname>/".  This handles the flat-namespace path
     * convention used by SimpleFS v1. */
    if (entry.type == FS_TYPE_DIR) {
        uint32_t prefix_len = strlen(name);
        for (uint32_t i = 0; i < FS_MAX_ENTRIES; i++) {
            fs_entry_t child;
            if (fs_read_entry(i, &child) != 0) continue;
            if (child.type == FS_TYPE_FREE) continue;

            /* Match "<name>/" prefix */
            if (strncmp(child.name, name, prefix_len) == 0 &&
                child.name[prefix_len] == '/') {
                /* Accumulate freed data blocks */
                if (child.type == FS_TYPE_FILE && child.size > 0) {
                    freed_blocks += (child.size + BLOCK_SIZE - 1) / BLOCK_SIZE;
                }
                /* Recurse for nested subdirectories */
                if (child.type == FS_TYPE_DIR) {
                    /* Wipe children of this sub-dir too (iterative: they
                     * will also match our prefix so this loop covers them) */
                }
                /* Clear the child entry */
                memset(&child, 0, sizeof(child));
                child.type = FS_TYPE_FREE;
                fs_write_entry(i, &child);
            }
        }
    }

    /* Clear the entry itself */
    memset(&entry, 0, sizeof(entry));
    entry.type = FS_TYPE_FREE;
    if (fs_write_entry(index, &entry) != 0) return -1;

    /* Update superblock */
    superblock.free_blocks += freed_blocks;
    fs_flush_superblock();

    return 0;
}

uint32_t fs_list(fs_entry_t* entries, uint32_t max) {
    if (!fs_dev || !entries) return 0;

    uint32_t count = 0;
    for (uint32_t i = 0; i < FS_MAX_ENTRIES && count < max; i++) {
        fs_entry_t e;
        if (fs_read_entry(i, &e) != 0) continue;
        if (e.type != FS_TYPE_FREE) {
            entries[count++] = e;
        }
    }
    return count;
}

int fs_stat(const char* name, fs_entry_t* out) {
    if (!fs_dev || !name || !out) return -1;
    return fs_find_entry(name, out, (uint32_t*)0);
}
