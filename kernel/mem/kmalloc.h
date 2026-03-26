#ifndef KMALLOC_H
#define KMALLOC_H

#include "../../klibc/stdint.h"
#include "../../klibc/stddef.h"

// Heap memory region (1MB - 2MB)
#define HEAP_START  0x100000  // 1MB
#define HEAP_END    0x200000  // 2MB
#define HEAP_SIZE   (HEAP_END - HEAP_START)

// Minimum allocation size (prevents tiny fragments)
#define MIN_ALLOC_SIZE 16

// Block header structure
typedef struct block_header {
    uint32_t size;      // Total size of block (including this header)
    uint32_t is_free;   // 1 = free, 0 = allocated
} block_header_t;

// Allocator functions
void kmalloc_init();                         // Initialize heap
void* kmalloc(uint32_t size);                // Allocate memory
void kfree(void* ptr);                       // Free memory
void kmalloc_status();                       // Print heap statistics (for debugging)

#endif