#include "kmalloc.h"
#include "../../drivers/screen.h"

void kmalloc_init() {
    block_header_t* initial_block = (block_header_t*)HEAP_START;
    initial_block->size = HEAP_SIZE;
    initial_block->is_free = 1;
}

void* kmalloc(uint32_t size) {
    // 1. Align size to 4-byte boundary
    if (size == 0) return NULL;
    if (size % 4 != 0) {
        size = size + (4 - size % 4);
    }
    
    // 2. Add header size
    uint32_t total_size = size + sizeof(block_header_t);
    
    // 3. Best-fit search - find smallest suitable block
    block_header_t* current = (block_header_t*)HEAP_START;
    block_header_t* best_fit = NULL;
    uint32_t best_fit_size = 0xFFFFFFFF;  // Start with max value
    
    // Scan entire heap to find best fit
    while ((uint32_t)current < HEAP_END) {
        if (current->is_free && current->size >= total_size) {
            // Found a suitable block
            if (current->size < best_fit_size) {
                // This block is better (smaller) than previous best
                best_fit = current;
                best_fit_size = current->size;
                
                // Perfect fit - can't do better than this
                if (best_fit_size == total_size) {
                    break;
                }
            }
        }
        
        // Move to next block
        current = (block_header_t*)((uint32_t)current + current->size);
    }
    
    // Check if we found a suitable block
    if (best_fit == NULL) {
        return NULL;  // No suitable block found
    }
    
    // 4. Split block if remainder is large enough
    if (best_fit->size >= total_size + sizeof(block_header_t) + MIN_ALLOC_SIZE) {
        block_header_t* new_block = (block_header_t*)((uint32_t)best_fit + total_size);
        new_block->size = best_fit->size - total_size;
        new_block->is_free = 1;
        best_fit->size = total_size;
    }
    
    // 5. Mark as allocated
    best_fit->is_free = 0;
    
    // 6. Return pointer to payload (skip header)
    return (void*)((uint32_t)best_fit + sizeof(block_header_t));
}

void kfree(void* ptr) {
    if (ptr == NULL) return;
    
    // 1. Get block header
    block_header_t* block = (block_header_t*)((uint32_t)ptr - sizeof(block_header_t));
    
    // 2. Mark as free
    block->is_free = 1;
    
    // 3. Forward coalescing - merge with next block if it's free
    block_header_t* next = (block_header_t*)((uint32_t)block + block->size);
    
    if ((uint32_t)next < HEAP_END && next->is_free) {
        block->size += next->size;  // Merge with next block
    }
    
    // 4. Backward coalescing - merge with previous block if it's free
    // Scan from heap start to find the block immediately before this one
    block_header_t* current = (block_header_t*)HEAP_START;
    block_header_t* prev = NULL;
    
    while ((uint32_t)current < HEAP_END && current != block) {
        prev = current;
        current = (block_header_t*)((uint32_t)current + current->size);
    }
    
    // If we found a previous block and it's free, merge
    if (prev != NULL && prev->is_free) {
        // Check that prev is actually adjacent to block
        if ((uint32_t)prev + prev->size == (uint32_t)block) {
            prev->size += block->size;  // Merge with previous block
            // Note: 'block' is now absorbed into 'prev', don't use it anymore
        }
    }
}

void kmalloc_status() {
    kprint("\n=== Heap Status ===\n");
    
    uint32_t total_blocks = 0;
    uint32_t free_blocks = 0;
    uint32_t allocated_blocks = 0;
    uint32_t total_free = 0;
    uint32_t total_allocated = 0;
    
    block_header_t* current = (block_header_t*)HEAP_START;
    
    while ((uint32_t)current < HEAP_END) {
        total_blocks++;
        
        if (current->is_free) {
            free_blocks++;
            total_free += current->size - sizeof(block_header_t);
        } else {
            allocated_blocks++;
            total_allocated += current->size - sizeof(block_header_t);
        }
        
        current = (block_header_t*)((uint32_t)current + current->size);
    }
    
    kprint("Total blocks: ");
    kprint_uint(total_blocks);
    kprint("\nFree blocks: ");
    kprint_uint(free_blocks);
    kprint("\nAllocated blocks: ");
    kprint_uint(allocated_blocks);
    kprint("\nFree memory: ");
    kprint_uint(total_free);
    kprint(" bytes\n");
    kprint("Allocated memory: ");
    kprint_uint(total_allocated);
    kprint(" bytes\n");
}
