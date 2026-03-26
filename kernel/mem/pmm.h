#ifndef PMM_H
#define PMM_H

#include "../../klibc/stdint.h"

/**
 * Physical Memory Manager (PMM)
 * 
 * Manages physical RAM at 4KB page granularity using a bitmap allocator.
 * Each bit in the bitmap represents one 4KB page frame.
 */

// Memory configuration
#define TOTAL_MEMORY_MB     16                          // Assume 16MB of RAM
#define TOTAL_MEMORY        (TOTAL_MEMORY_MB * 1024 * 1024)
#define PAGE_SIZE           4096                        // 4KB pages
#define TOTAL_FRAMES        (TOTAL_MEMORY / PAGE_SIZE)  // 4096 frames

// PMM bitmap location (placed at 2MB, after kernel heap)
#define PMM_BITMAP_ADDRESS  0x200000

// Page allocation pool (starts after bitmap, ends before device memory)
#define PMM_POOL_START      0x201000    // First allocatable page (~2MB)
#define PMM_POOL_END        0xF00000    // Last allocatable page (15MB)

// Bitmap manipulation macros
#define BITMAP_INDEX(frame)  ((frame) / 32)  // Which uint32_t in bitmap array
#define BITMAP_OFFSET(frame) ((frame) % 32)  // Which bit in that uint32_t

/**
 * pmm_init - Initialize the Physical Memory Manager
 * 
 * Sets up the bitmap allocator and marks reserved memory regions.
 * Must be called during kernel initialization before any page allocations.
 */
void pmm_init(void);

/**
 * alloc_page - Allocate a single 4KB physical page frame
 * 
 * Searches the bitmap for a free page frame, marks it as used,
 * and returns its physical address.
 * 
 * Returns: Physical address of allocated page (page-aligned), or 0 if out of memory
 */
uint32_t alloc_page(void);

/**
 * alloc_pages - Allocate multiple contiguous physical page frames
 * @count: Number of consecutive pages to allocate
 * 
 * Searches for 'count' consecutive free page frames, marks them as used,
 * and returns the physical address of the first page.
 * 
 * Returns: Physical address of first page, or 0 if can't find contiguous block
 */
uint32_t alloc_pages(uint32_t count);

/**
 * free_page - Free a physical page frame
 * @addr: Physical address of page to free (must be page-aligned)
 * 
 * Marks the page frame as free in the bitmap.
 * Includes validation and double-free detection.
 */
void free_page(uint32_t addr);

/**
 * pmm_status - Display physical memory statistics
 * 
 * Prints total/used/free memory information to the screen.
 * Useful for debugging and monitoring memory usage.
 */
void pmm_status(void);

/**
 * get_total_memory - Get total physical memory in bytes
 * 
 * Returns: Total amount of physical RAM
 */
uint32_t get_total_memory(void);

/**
 * get_used_memory - Get used physical memory in bytes
 * 
 * Returns: Amount of physical RAM currently allocated
 */
uint32_t get_used_memory(void);

/**
 * get_free_memory - Get free physical memory in bytes
 * 
 * Returns: Amount of physical RAM currently available
 */
uint32_t get_free_memory(void);

#endif // PMM_H
