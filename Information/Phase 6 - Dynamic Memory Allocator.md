# Phase 6: Dynamic Memory Allocator (Heap)

## Overview
Implement a kernel heap allocator to enable dynamic memory allocation in the kernel. This provides `kmalloc()` and `kfree()` functions similar to userspace `malloc()` and `free()`.

**Goal:** Allocate and free variable-sized memory blocks at runtime.

**Why we need this:**
- Shell command parsing (dynamic string buffers)
- File system metadata (directory entries, file handles)
- Network packet buffers
- Data structures that grow/shrink (linked lists, trees)
- Future process control blocks

---

## Theory: How Heap Allocators Work

### Memory Layout
```
Low Memory                                               High Memory
|-------|-----------|----------------------|-----------|---------|
| 0-1MB | Kernel    | Heap Region (Free)   | Stack     | Video   |
|       | Code/Data |                      | (grows ↓) | Memory  |
|-------|-----------|----------------------|-----------|---------|
                    ↑                      ↑
               HEAP_START              HEAP_END
```

### Block Structure (Metadata + Payload)
Each allocated block has a header:
```
+-------------------+
| Block Header      |  8 bytes
|  - size           |  4 bytes (includes header size)
|  - is_free        |  4 bytes (1 = free, 0 = allocated)
+-------------------+
| User Data         |  N bytes (what kmalloc returns pointer to)
| (payload)         |
+-------------------+
```

### Allocation Strategies

**Best-Fit (Recommended for Phase 6):**
```c
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
```

- Scan entire free list
- Return smallest block >= requested size
- Minimizes fragmentation over time
- Slightly slower than first-fit, but better memory efficiency

**First-Fit:**
- Scan free list from start
- Return first block >= requested size
- Split block if much larger than needed
- Simple, predictable, faster but more fragmentation

**Next-Fit:**
- Like first-fit, but start search from last allocation
- Spreads allocations across heap

---

## Implementation Plan

### Phase 6.1: Basic Infrastructure ✅ COMPLETED

| Task | File | Description | Status |
|------|------|-------------|--------|
| 6.1.1 | `kernel/kmalloc.h` | Define constants: `HEAP_START`, `HEAP_END`, `HEAP_SIZE` | ✅ |
| 6.1.2 | `kernel/kmalloc.h` | Define `block_header` struct (size, is_free) | ✅ |
| 6.1.3 | `kernel/kmalloc.h` | Declare function prototypes: `kmalloc()`, `kfree()`, `kmalloc_init()`, `kmalloc_status()` | ✅ |
| 6.1.4 | `kernel/kmalloc.c` | Create source file with includes | ✅ |

**Expected `kmalloc.h`:**
```c
#ifndef KMALLOC_H
#define KMALLOC_H

#include "../libc/stdint.h"

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
```

### Phase 6.2: Heap Initialization ✅ COMPLETED

| Task | File | Description | Status |
|------|------|-------------|--------|
| 6.2.1 | `kernel/kmalloc.c` | Write `kmalloc_init()` - set up initial free block | ✅ |
| 6.2.2 | `kernel/kmalloc.c` | Create first block spanning entire heap | ✅ |
| 6.2.3 | `kernel/kmalloc.c` | Set block size = `HEAP_SIZE`, is_free = 1 | ✅ |
| 6.2.4 | `kernel/kernel_main.c` | Call `kmalloc_init()` during kernel startup | ✅ |

**Implementation:**
```c
void kmalloc_init() {
    block_header_t* initial_block = (block_header_t*)HEAP_START;
    initial_block->size = HEAP_SIZE;
    initial_block->is_free = 1;
}
```

**Integration in `kernel_main.c`:**
```c
void main() {
    clear_screen();
    kprint("ApPa Kernel v0.1\n");
    
    // ... existing IDT, PIC, keyboard init ...
    
    // NEW: Initialize heap
    kmalloc_init();
    kprint("  [OK] Heap initialized\n");
    
    // ... rest of initialization ...
}
```

### Phase 6.3: Memory Allocation (kmalloc) ✅ COMPLETED

| Task | File | Description | Status |
|------|------|-------------|--------|
| 6.3.1 | `kernel/kmalloc.c` | Start `kmalloc()` function - align size to 4 bytes | ✅ |
| 6.3.2 | `kernel/kmalloc.c` | Add header size to requested size | ✅ |
| 6.3.3 | `kernel/kmalloc.c` | Implement best-fit search algorithm | ✅ |
| 6.3.4 | `kernel/kmalloc.c` | Split large blocks if remainder > MIN_ALLOC_SIZE | ✅ |
| 6.3.5 | `kernel/kmalloc.c` | Mark block as allocated, return payload pointer | ✅ |
| 6.3.6 | `kernel/kmalloc.c` | Handle allocation failure (return NULL) | ✅ |

**Algorithm:**
```c
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
```

### Phase 6.4: Memory Deallocation (kfree) ✅ COMPLETED

| Task | File | Description | Status |
|------|------|-------------|--------|
| 6.4.1 | `kernel/kmalloc.c` | Start `kfree()` - validate pointer is not NULL | ✅ |
| 6.4.2 | `kernel/kmalloc.c` | Get block header from user pointer | ✅ |
| 6.4.3 | `kernel/kmalloc.c` | Mark block as free | ✅ |
| 6.4.4 | `kernel/kmalloc.c` | Implement forward coalescing - merge with next block if free | ✅ |
| 6.4.5 | `kernel/kmalloc.c` | Implement backward coalescing - merge with previous block if free | ✅ |

**Algorithm:**
```c
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
```

### Phase 6.5: Debugging & Statistics ✅ COMPLETED

| Task | File | Description | Status |
|------|------|-------------|--------|
| 6.5.1 | `kernel/kmalloc.c` | Write `kmalloc_status()` - scan heap and print stats | ✅ |
| 6.5.2 | `kernel/kmalloc.c` | Count total blocks, free blocks, allocated blocks | ✅ |
| 6.5.3 | `kernel/kmalloc.c` | Calculate total free memory, total allocated memory | ✅ |
| 6.5.4 | `kernel/kmalloc.c` | Print fragmentation info (number of free blocks) | ✅ |

**Implementation:**
```c
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
```

### Phase 6.6: Testing ✅ COMPLETED

| Task | File | Description | Status |
|------|------|-------------|--------|
| 6.6.1 | `kernel/kernel_main.c` | Test 1: Single allocation and free | ✅ |
| 6.6.2 | `kernel/kernel_main.c` | Test 2: Multiple allocations | ✅ |
| 6.6.3 | `kernel/kernel_main.c` | Test 3: Write data to allocated memory | ✅ |
| 6.6.4 | `kernel/kernel_main.c` | Test 4: Test forward and backward coalescing | ✅ |
| 6.6.5 | `kernel/kernel_main.c` | Test 5: Exhaust heap (handle NULL return) | ✅ |
| 6.6.6 | Remove test code | Clean up kernel_main.c after verification | ✅ |

**Test Code Examples:**
```c
void test_kmalloc() {
    kprint("\n=== Testing kmalloc ===\n");
    
    // Test 1: Basic allocation
    kprint("\nTest 1: Allocate 64 bytes\n");
    char* buf1 = (char*)kmalloc(64);
    if (buf1 != NULL) {
        kprint("  [OK] Allocation successful\n");
        // Write data
        for (int i = 0; i < 10; i++) {
            buf1[i] = 'A' + i;
        }
        buf1[10] = '\0';
        kprint("  Data: ");
        kprint(buf1);
        kprint("\n");
    } else {
        kprint("  [FAIL] Allocation failed!\n");
    }
    
    // Test 2: Multiple allocations
    kprint("\nTest 2: Multiple allocations\n");
    int* nums = (int*)kmalloc(sizeof(int) * 5);
    char* buf2 = (char*)kmalloc(128);
    char* buf3 = (char*)kmalloc(32);
    
    if (nums && buf2 && buf3) {
        kprint("  [OK] All allocations successful\n");
    }
    
    // Show heap status
    kmalloc_status();
    
    // Test 3: Free memory (tests forward coalescing)
    kprint("\nTest 3: Free first buffer\n");
    kfree(buf1);
    kmalloc_status();
    
    // Test 4: Test backward coalescing
    // Free in reverse order: nums is before buf1 in memory
    // This should trigger backward coalescing
    kprint("\nTest 4: Free nums (tests backward coalescing)\n");
    kfree(nums);  // Should merge with the free buf1 block
    kmalloc_status();
    
    // Test 5a: Reallocation (should reuse merged space)
    kprint("\nTest 5a: Allocate 32 bytes (should reuse merged space)\n");
    char* buf4 = (char*)kmalloc(32);
    kmalloc_status();
    
    // Test 5b: Test forward coalescing - free adjacent blocks
    kprint("\nTest 5b: Free buf2 then buf3 (forward coalescing)\n");
    kfree(buf2);  // buf2 is freed
    kmalloc_status();
    kfree(buf3);  // buf3 should merge with buf2 via backward coalescing
    kmalloc_status();
    
    // Cleanup
    kfree(buf4);
    
    kprint("\nAfter freeing all:\n");
    kmalloc_status();
}
```

### Phase 6.7: Build Integration ✅ COMPLETED

| Task | File | Description | Status |
|------|------|-------------|--------|
| 6.7.1 | `makefile` | Add `kernel/kmalloc.c` to C_SOURCES | ✅ |
| 6.7.2 | `makefile` | Verify kernel builds successfully | ✅ |
| 6.7.3 | `makefile` | Test in QEMU - run test suite | ✅ |

---

## File Structure After Completion

```
kernel/
    kmalloc.c          # NEW - Heap allocator implementation
    kmalloc.h          # NEW - Allocator interface
    idt.c / idt.h
    isr_stubs.asm
    isr.c / isr.h
    irq_stubs.asm
    irq.c / irq.h
    pic.c / pic.h
    kernel_main.c      # MODIFIED - Add kmalloc_init() call
```

---

## Common Pitfalls & Solutions

### 1. **Integer Overflow in Pointer Arithmetic**
**Problem:** `(block_header_t*)block + block->size` adds `size * sizeof(block_header_t)`, not `size` bytes!

**Solution:** Cast to `uint32_t` first:
```c
next = (block_header_t*)((uint32_t)current + current->size);
```

### 2. **Forgetting to Include Header Size**
**Problem:** Allocating exactly the user's requested size without accounting for the header.

**Solution:** Always add `sizeof(block_header_t)` to requested size.

### 3. **Not Checking for NULL After kmalloc()**
**Problem:** Dereferencing NULL pointer when heap is exhausted.

**Solution:**
```c
char* buf = (char*)kmalloc(1024);
if (buf == NULL) {
    kprint("ERROR: Out of memory!\n");
    return;
}
// Now safe to use buf
```

### 4. **Double Free**
**Problem:** Calling `kfree()` twice on same pointer causes corruption.

**Solution:** Set pointer to NULL after freeing:
```c
kfree(ptr);
ptr = NULL;
```

### 5. **Heap Corruption Detection**
**Problem:** Buffer overruns can overwrite block headers.

**Solution (Advanced):** Add magic numbers to headers:
```c
typedef struct block_header {
    uint32_t magic;     // 0xDEADBEEF
    uint32_t size;
    uint32_t is_free;
} block_header_t;

// In kmalloc_init() and kmalloc():
block->magic = 0xDEADBEEF;

// In kfree(), verify:
if (block->magic != 0xDEADBEEF) {
    kprint("ERROR: Heap corruption detected!\n");
    return;
}
```

### 6. **Memory Leaks**
**Problem:** Allocating memory but never freeing it.

**Solution:** Use `kmalloc_status()` regularly during development to track allocations.

---

## Execution Order at Runtime

```
main()
  → idt_init()
  → pic_remap()
  → kmalloc_init()      # NEW - One-time heap setup
  → keyboard_init()
  → sti
  → [kernel runs]
       → kmalloc(64)    # User allocates memory
       → kfree(ptr)     # User frees memory
```

---

## Verification Checklist

All items verified and completed:

- [x] Heap initializes without errors
- [x] Single allocation succeeds and returns valid pointer
- [x] Can write to allocated memory without crash
- [x] Can allocate multiple blocks
- [x] Freeing memory marks block as free
- [x] Forward coalescing merges with next free block
- [x] Backward coalescing merges with previous free block
- [x] `kmalloc_status()` shows correct statistics
- [x] Allocation fails gracefully when heap exhausted (returns NULL)
- [x] No kernel panics or triple faults during tests

---

## Next Steps After Phase 6

Once the heap allocator is working:
1. **Remove test code** from `kernel_main.c`
2. **Move on to Phase 7:** Simple Command Shell
   - Use `kmalloc()` for command buffers
   - Add `mem` command that calls `kmalloc_status()`
3. **Extend allocator (optional):**
   - Add `kcalloc()` - allocate + zero memory
   - Add `krealloc()` - resize allocation

---

## Additional Resources

**Heap Allocator Algorithms:**
- Best-Fit (implemented here)
- First-Fit
- Buddy System (used by Linux for page allocation)
- Slab Allocator (caching for fixed-size objects)

**Debugging Tools to Add Later:**
- Heap visualization (`kmalloc_dump()` - print all blocks)
- Allocation tracking (source file + line number)
- Memory leak detector

**Advanced Features:**
- Thread-safe allocator (when multitasking is implemented)
- Separate user heap vs kernel heap
- Memory pool allocator for fixed-size objects (faster than general allocator)
