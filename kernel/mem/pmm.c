/**
 * pmm.c - Physical Memory Manager
 * 
 * Manages physical RAM at 4KB page granularity using a bitmap allocator.
 * Each bit in the bitmap represents the allocation status of one page frame.
 */

#include "pmm.h"
#include "../../drivers/screen.h"
#include "../../klibc/string.h"
#include "kmalloc.h"

// PMM state structure
typedef struct {
    uint32_t total_frames;    // Total number of page frames
    uint32_t used_frames;     // Number of currently allocated frames
    uint32_t* bitmap;         // Pointer to bitmap array
    uint32_t bitmap_size;     // Size of bitmap in uint32_t units
} pmm_t;

// Global PMM instance
static pmm_t pmm;

/**
 * pmm_set_frame - Mark a page frame as used
 * @frame: Frame number to mark as used
 */
static inline void pmm_set_frame(uint32_t frame) {
    uint32_t idx = BITMAP_INDEX(frame);
    uint32_t off = BITMAP_OFFSET(frame);
    pmm.bitmap[idx] |= (1 << off);
    pmm.used_frames++;
}

/**
 * pmm_clear_frame - Mark a page frame as free
 * @frame: Frame number to mark as free
 */
static inline void pmm_clear_frame(uint32_t frame) {
    uint32_t idx = BITMAP_INDEX(frame);
    uint32_t off = BITMAP_OFFSET(frame);
    pmm.bitmap[idx] &= ~(1 << off);
    pmm.used_frames--;
}

/**
 * pmm_test_frame - Check if a page frame is in use
 * @frame: Frame number to test
 * 
 * Returns: 1 if frame is used, 0 if free
 */
static inline uint32_t pmm_test_frame(uint32_t frame) {
    uint32_t idx = BITMAP_INDEX(frame);
    uint32_t off = BITMAP_OFFSET(frame);
    return (pmm.bitmap[idx] & (1 << off)) != 0;
}

/**
 * pmm_first_free_frame - Find the first free frame
 * 
 * Returns: Frame number of first free frame, or TOTAL_FRAMES if none found
 */
static uint32_t pmm_first_free_frame(void) {
    for (uint32_t i = 0; i < pmm.bitmap_size; i++) {
        if (pmm.bitmap[i] != 0xFFFFFFFF) {  // Not all bits are set
            // Found a uint32_t with at least one free bit
            for (uint32_t j = 0; j < 32; j++) {
                uint32_t bit = 1 << j;
                if ((pmm.bitmap[i] & bit) == 0) {  // Found free bit
                    return i * 32 + j;
                }
            }
        }
    }
    return TOTAL_FRAMES;  // No free frames
}

/**
 * pmm_mark_reserved_regions - Mark reserved memory regions as used
 * 
 * Reserves:
 *   - Low memory (0-1MB): BIOS, bootloader, kernel, stack
 *   - Kernel heap (1MB-2MB): kmalloc region
 *   - PMM bitmap itself
 *   - High memory (15MB-16MB): Memory-mapped I/O region
 */
static void pmm_mark_reserved_regions(void) {
    // Reserve low memory (0 - 1MB): BIOS, bootloader, kernel, stack
    // Frames 0-255 = 0x00000000 to 0x000FFFFF
    for (uint32_t frame = 0; frame < 256; frame++) {
        pmm_set_frame(frame);
    }
    
    // Reserve kernel heap region (1MB - 2MB): kmalloc region
    // Frames 256-511 = 0x00100000 to 0x001FFFFF
    for (uint32_t frame = 256; frame < 512; frame++) {
        pmm_set_frame(frame);
    }
    
    // Reserve PMM bitmap itself
    uint32_t bitmap_start_frame = (uint32_t)pmm.bitmap / PAGE_SIZE;
    uint32_t bitmap_bytes = pmm.bitmap_size * sizeof(uint32_t);
    uint32_t bitmap_frames = (bitmap_bytes / PAGE_SIZE) + 1;
    for (uint32_t i = 0; i < bitmap_frames; i++) {
        pmm_set_frame(bitmap_start_frame + i);
    }
    
    // Reserve high memory (15MB - 16MB): Memory-mapped I/O region
    // Frames 3840-4095 = 0x00F00000 to 0x00FFFFFF
    for (uint32_t frame = 3840; frame < 4096; frame++) {
        pmm_set_frame(frame);
    }
}

/**
 * pmm_init - Initialize the Physical Memory Manager
 */
void pmm_init(void) {
    // Calculate number of frames
    pmm.total_frames = TOTAL_FRAMES;
    pmm.used_frames = 0;
    
    // Calculate bitmap size in uint32_t units
    // Need 1 bit per frame, so total_frames bits
    // Each uint32_t holds 32 bits, so divide by 32 (round up)
    pmm.bitmap_size = (pmm.total_frames + 31) / 32;
    
    // Place bitmap at fixed address (2MB, after kernel heap)
    pmm.bitmap = (uint32_t*)PMM_BITMAP_ADDRESS;
    
    // Initialize all frames as free (0)
    memset(pmm.bitmap, 0, pmm.bitmap_size * sizeof(uint32_t));
    
    // Mark reserved regions as used
    pmm_mark_reserved_regions();
}

/**
 * alloc_page - Allocate a single 4KB physical page frame
 * 
 * Returns: Physical address of allocated page, or 0 if out of memory
 */
uint32_t alloc_page(void) {
    uint32_t frame = pmm_first_free_frame();
    
    if (frame == TOTAL_FRAMES) {
        // No free pages available
        return 0;
    }
    
    // Mark frame as used
    pmm_set_frame(frame);
    
    // Return physical address
    return frame * PAGE_SIZE;
}

/**
 * alloc_pages - Allocate multiple contiguous physical page frames
 * @count: Number of consecutive pages to allocate
 * 
 * Returns: Physical address of first page, or 0 if can't find contiguous block
 */
uint32_t alloc_pages(uint32_t count) {
    if (count == 0) {
        return 0;
    }
    
    if (count == 1) {
        return alloc_page();  // Optimize single page allocation
    }
    
    uint32_t start_frame = 0;
    uint32_t consecutive = 0;
    
    // Scan for 'count' consecutive free frames
    for (uint32_t frame = 0; frame < pmm.total_frames; frame++) {
        if (!pmm_test_frame(frame)) {  // Frame is free
            if (consecutive == 0) {
                start_frame = frame;  // Mark potential start
            }
            consecutive++;
            
            if (consecutive == count) {
                // Found enough consecutive frames
                // Mark them all as used
                for (uint32_t i = 0; i < count; i++) {
                    pmm_set_frame(start_frame + i);
                }
                return start_frame * PAGE_SIZE;
            }
        } else {
            // Chain broken, reset
            consecutive = 0;
        }
    }
    
    return 0;  // Couldn't find contiguous block
}

/**
 * free_page - Free a physical page frame
 * @addr: Physical address of page to free (must be page-aligned)
 */
void free_page(uint32_t addr) {
    // Validate address is page-aligned
    if (addr % PAGE_SIZE != 0) {
        kprint("ERROR: free_page() - Address not page-aligned: 0x");
        kprint_hex(addr);
        kprint("\n");
        return;
    }
    
    // Calculate frame number
    uint32_t frame = addr / PAGE_SIZE;
    
    // Validate frame is in range
    if (frame >= pmm.total_frames) {
        kprint("ERROR: free_page() - Frame out of range: ");
        kprint_uint(frame);
        kprint("\n");
        return;
    }
    
    // Check if already free (double-free detection)
    if (!pmm_test_frame(frame)) {
        kprint("WARNING: free_page() - Double free detected at 0x");
        kprint_hex(addr);
        kprint("\n");
        return;
    }
    
    // Mark as free
    pmm_clear_frame(frame);
}

/**
 * pmm_status - Display physical memory statistics
 */
void pmm_status(void) {
    uint32_t total_kb = (pmm.total_frames * PAGE_SIZE) / 1024;
    uint32_t used_kb = (pmm.used_frames * PAGE_SIZE) / 1024;
    uint32_t free_kb = total_kb - used_kb;
    uint32_t free_frames = pmm.total_frames - pmm.used_frames;
    
    kprint("Physical Memory Manager Status:\n");
    
    kprint("  Total Memory:  ");
    kprint_uint(total_kb);
    kprint(" KB (");
    kprint_uint(pmm.total_frames);
    kprint(" frames)\n");
    
    kprint("  Used Memory:   ");
    kprint_uint(used_kb);
    kprint(" KB (");
    kprint_uint(pmm.used_frames);
    kprint(" frames)\n");
    
    kprint("  Free Memory:   ");
    kprint_uint(free_kb);
    kprint(" KB (");
    kprint_uint(free_frames);
    kprint(" frames)\n");
    
    kprint("  Page Size:     ");
    kprint_uint(PAGE_SIZE / 1024);
    kprint(" KB\n");
    
    kprint("  Bitmap Size:   ");
    kprint_uint((pmm.bitmap_size * sizeof(uint32_t)));
    kprint(" bytes (");
    kprint_uint(pmm.bitmap_size);
    kprint(" uint32_t)\n");
}

/**
 * get_total_memory - Get total physical memory in bytes
 */
uint32_t get_total_memory(void) {
    return pmm.total_frames * PAGE_SIZE;
}

/**
 * get_used_memory - Get used physical memory in bytes
 */
uint32_t get_used_memory(void) {
    return pmm.used_frames * PAGE_SIZE;
}

/**
 * get_free_memory - Get free physical memory in bytes
 */
uint32_t get_free_memory(void) {
    return (pmm.total_frames - pmm.used_frames) * PAGE_SIZE;
}
