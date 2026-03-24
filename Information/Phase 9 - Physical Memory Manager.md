# Phase 9: Physical Memory Manager (PMM)

## Overview
Implement a physical page frame allocator to manage the physical RAM available to the operating system. This provides the foundation for virtual memory (paging) by tracking which 4KB physical pages are free or in use.

**Goal:** Track and allocate physical memory at page granularity (4KB blocks).

**Why we need this:**
- **Required for paging/virtual memory** - Need physical frames for page tables and page directories
- **Process isolation** - Each process needs its own physical memory pages
- **Memory protection** - Track which physical pages are mapped where
- **Efficient allocation** - Quickly find and allocate contiguous or scattered physical memory
- **Foundation for advanced features** - Shared memory, memory-mapped I/O, DMA buffers

---

## Theory: Physical vs Virtual Memory

### Physical Memory
**Physical memory** is the actual RAM chips in the computer. Each byte has a unique physical address that directly maps to a location in the RAM hardware.

```
Physical RAM (e.g., 16MB system)
┌────────────────────────────────────────────────┐
│ 0x00000000 - 0x00FFFFFF (16,777,216 bytes)    │
│                                                │
│ Divided into 4,096 page frames of 4KB each:   │
│   Frame 0: 0x00000000 - 0x00000FFF             │
│   Frame 1: 0x00001000 - 0x00001FFF             │
│   Frame 2: 0x00002000 - 0x00002FFF             │
│   ...                                          │
│   Frame 4095: 0x00FFF000 - 0x00FFFFFF          │
└────────────────────────────────────────────────┘
```

### Virtual Memory (Coming in Phase 10)
**Virtual memory** is an abstraction layer. Programs use virtual addresses that get translated to physical addresses by the CPU's Memory Management Unit (MMU).

```
Virtual Address Space (4GB on x86)       Physical RAM (16MB)
┌─────────────────────────┐            ┌──────────────┐
│ 0x00000000 - 0xFFFFFFFF │───────┐    │ 0x00000000   │
│                         │       │    │   ...        │
│ Process can "see"       │       └───>│ 0x00123000   │ ← Actual RAM
│ 4GB of memory even      │            │   ...        │
│ though system only      │            │ 0x00FFFFFF   │
│ has 16MB of RAM!        │            └──────────────┘
└─────────────────────────┘
      (Translation via Page Tables)
```

### Why 4KB Pages?
- **x86 standard** - Hardware MMU works with 4KB page granularity
- **Good balance** - Small enough for efficient memory use, large enough to minimize overhead
- **Page table structure** - Each page table entry (PTE) maps exactly 4KB
- **TLB efficiency** - Translation Lookaside Buffer (CPU cache) optimized for 4KB pages

---

## Memory Regions in a Typical x86 System

### Low Memory (0 - 1MB) - "Conventional Memory"
```
0x00000000 - 0x000003FF  | Real Mode IVT (Interrupt Vector Table)     | 1 KB
0x00000400 - 0x000004FF  | BIOS Data Area (BDA)                       | 256 B
0x00000500 - 0x00007BFF  | Free (usable by OS)                        | ~30 KB
0x00007C00 - 0x00007DFF  | Bootloader (loaded by BIOS)                | 512 B
0x00007E00 - 0x0007FFFF  | Free (usable by OS)                        | ~480 KB
0x00080000 - 0x0009FFFF  | EBDA (Extended BIOS Data Area)             | 128 KB
0x000A0000 - 0x000BFFFF  | Video Memory (VGA text mode at 0xB8000)    | 128 KB
0x000C0000 - 0x000FFFFF  | BIOS ROM and Video BIOS                    | 256 KB
```

### Extended Memory (Above 1MB)
```
0x00100000 - 0x00EFFFFF  | Extended Memory (safe to use)              | ~14 MB
0x00F00000 - 0x00FFFFFF  | Reserved for memory-mapped devices         | 1 MB
0x01000000 - (varies)    | More RAM if system has > 16MB              | Varies
```

### Our Kernel's Memory Layout
```
0x00000000 - 0x00000FFF  | Bootloader and BIOS data                   | 4 KB
0x00001000 - 0x0000XXXX  | Kernel code/data (loaded here)             | ~25 KB
0x0000XXXX - 0x000FFFFF  | Kernel stack (grows downward)              | ~600 KB
0x00100000 - 0x001FFFFF  | Kernel heap (kmalloc region)               | 1 MB
0x00200000 - 0x00EFFFFF  | **Free for page allocation** ← PMM manages | ~13 MB
0x00F00000+              | Reserved/Device memory                     | -
```

---

## Page Frame Allocator Design

### Data Structures

#### Option 1: Bitmap Allocator (Recommended)
**Pros:**
- Simple to implement
- Fast lookup (bit operations)
- Constant memory overhead
- Easy to visualize

**Cons:**
- O(n) scan to find free page
- Wastes bits for reserved regions

```c
// ============================================================================
// WHY uint32_t* FOR THE BITMAP?
// ============================================================================
// 
// We can't work with individual bits in C (smallest unit is a byte).
// We use uint32_t instead of uint8_t because:
//   1. x86 is a 32-bit architecture - native operations are 32-bit
//   2. Better performance - fewer array accesses (32 bits at once vs 8)
//   3. Better alignment - CPU reads 4-byte-aligned data efficiently
//   4. Fewer total array elements = better cache locality
//
// ============================================================================
// WHAT DOES THE BITMAP HOLD?
// ============================================================================
//
// Each BIT (not byte!) represents ONE page frame:
//   Bit 0 = Page frame is FREE (available)
//   Bit 1 = Page frame is USED (allocated or reserved)
//
// For 16MB of RAM:
//   - 16MB ÷ 4KB = 4096 page frames
//   - Need 4096 bits to track all frames
//   - 4096 bits ÷ 32 bits/uint32_t = 128 uint32_t values
//   - Total bitmap size = 128 × 4 bytes = 512 bytes
//
// Example bitmap layout:
//
//   Physical Memory:     ┌─ Frame 0 (0x00000000-0x00000FFF)
//                        ├─ Frame 1 (0x00001000-0x00001FFF)
//                        ├─ Frame 2 (0x00002000-0x00002FFF)
//                        ├─ ...
//                        └─ Frame 4095 (0x00FFF000-0x00FFFFFF)
//
//   Bitmap in memory:
//   
//   bitmap[0] = 0b00000000000000000000000011111111 (frames 0-31)
//                                          ↑↑↑↑↑↑↑↑
//                                          ││││││││
//                   Frames 7-0: ALL USED ──┘│││││││
//                   (kernel code/BIOS)       │││││││
//                   Frames 31-8: ALL FREE ───┘┘┘┘┘┘┘
//   
//   bitmap[1] = 0b11111111111111111111111111111111 (frames 32-63)
//               (all 32 frames are USED - heap region)
//   
//   bitmap[2] = 0b00000000000000000000000000000000 (frames 64-95)
//               (all 32 frames are FREE - available for allocation)
//
// ============================================================================
// MEMORY EFFICIENCY:
// ============================================================================
//
// To track 16MB of RAM (4096 pages):
//   - Using uint32_t: 128 array elements × 4 bytes = 512 bytes ✓
//   - Using uint8_t:  512 array elements × 1 byte = 512 bytes (same size)
//   - Using uint16_t: 256 array elements × 2 bytes = 512 bytes (same size)
//
// Same memory usage, but uint32_t gives better performance on x86!
//
// ============================================================================

typedef struct {
    uint32_t total_frames;        // Total number of page frames
    uint32_t used_frames;         // Number of currently allocated frames
    uint32_t* bitmap;             // Pointer to array of uint32_t (each holds 32 bits)
    uint32_t bitmap_size;         // Size of bitmap in uint32_t units (NOT bytes!)
} pmm_t;

// Each bit in bitmap represents one page frame
#define BITMAP_INDEX(frame)  ((frame) / 32)  // Which uint32_t in array
#define BITMAP_OFFSET(frame) ((frame) % 32)  // Which bit in that uint32_t

// Example: Finding the location of frame 100
//   BITMAP_INDEX(100)  = 100 / 32 = 3  → bitmap[3]
//   BITMAP_OFFSET(100) = 100 % 32 = 4  → bit 4 within bitmap[3]
//
// So frame 100's status is stored in bit 4 of bitmap[3]:
//   bitmap[3] = 0b????????????????????????000?????10000
//                                        ↑
//                                      bit 4 (frame 100)

// Set a page frame as used (allocate)
static inline void pmm_set_frame(uint32_t frame) {
    uint32_t idx = BITMAP_INDEX(frame);    // Which uint32_t?
    uint32_t off = BITMAP_OFFSET(frame);   // Which bit?
    pmm.bitmap[idx] |= (1 << off);         // Set bit to 1 (OR operation)
    pmm.used_frames++;
}
// Example: pmm_set_frame(100)
//   idx = 3, off = 4
//   bitmap[3] |= (1 << 4)
//   bitmap[3] |= 0b00000000000000000000000000010000
//   Before: 0b00000000000000000000000000000000 (frame 100 was free)
//   After:  0b00000000000000000000000000010000 (frame 100 now used)

// Set a page frame as free (deallocate)
static inline void pmm_clear_frame(uint32_t frame) {
    uint32_t idx = BITMAP_INDEX(frame);
    uint32_t off = BITMAP_OFFSET(frame);
    pmm.bitmap[idx] &= ~(1 << off);        // Clear bit to 0 (AND with inverted mask)
    pmm.used_frames--;
}
// Example: pmm_clear_frame(100)
//   idx = 3, off = 4
//   bitmap[3] &= ~(1 << 4)
//   bitmap[3] &= ~0b00000000000000000000000000010000
//   bitmap[3] &=  0b11111111111111111111111111101111  (inverted mask)
//   Before: 0b00000000000000000000000000010000 (frame 100 was used)
//   After:  0b00000000000000000000000000000000 (frame 100 now free)

// Test if a page frame is in use
static inline uint32_t pmm_test_frame(uint32_t frame) {
    uint32_t idx = BITMAP_INDEX(frame);
    uint32_t off = BITMAP_OFFSET(frame);
    return (pmm.bitmap[idx] & (1 << off)) != 0;  // AND with mask, check if non-zero
}
// Example: pmm_test_frame(100)
//   idx = 3, off = 4
//   (bitmap[3] & (1 << 4)) != 0
//   (bitmap[3] & 0b00000000000000000000000000010000) != 0
//   If bitmap[3] = 0b00000000000000000000000000010000:
//     Result: 0b00000000000000000000000000010000 != 0 → TRUE (frame is used)
//   If bitmap[3] = 0b00000000000000000000000000000000:
//     Result: 0b00000000000000000000000000000000 != 0 → FALSE (frame is free)
```

**How it works:**
```
Memory: 0x00000000 ────────────> 0x00FFFFFF (16MB)
        └─────┬─────┘ └─────┬─────┘ ...
           Frame 0       Frame 1

Bitmap: [uint32_t] [uint32_t] [uint32_t] ...
         ↓           ↓           ↓
         Frames      Frames      Frames
         0-31        32-63       64-95

Example allocation:
  alloc_page() → returns 0x00002000 (frame 2)
  
  Before:  bitmap[0] = 0b00000000000000000000000000000000
  After:   bitmap[0] = 0b00000000000000000000000000000100
                                                       ↑
                                                    Frame 2 = 1 (used)
```

#### Option 2: Stack Allocator
**Pros:**
- O(1) allocation (pop from stack)
- O(1) deallocation (push to stack)
- Very fast

**Cons:**
- Non-deterministic (can't allocate specific frame)
- More memory overhead (4 bytes per frame)
- Stack must be pre-initialized

```c
typedef struct {
    uint32_t* stack;              // Array of free frame numbers
    uint32_t stack_top;           // Index of top element
    uint32_t total_frames;
} pmm_stack_t;

// Allocate (pop from stack)
uint32_t alloc_page(void) {
    if (stack_top == 0) return 0;  // No free pages
    return stack[--stack_top];
}

// Free (push to stack)
void free_page(uint32_t frame) {
    stack[stack_top++] = frame;
}
```

---

## Implementation Steps

### Step 1: Memory Detection
**Option A:** Use BIOS INT 0x15, EAX=0xE820 (best, but complex)
- Call from bootloader before entering protected mode
- Returns memory map with all regions
- Store in a fixed location for kernel to read

**Option B:** Assume fixed size (simpler, good for learning)
```c
#define TOTAL_MEMORY_MB  16      // Assume 16MB of RAM
#define TOTAL_MEMORY     (TOTAL_MEMORY_MB * 1024 * 1024)
#define PAGE_SIZE        4096
#define TOTAL_FRAMES     (TOTAL_MEMORY / PAGE_SIZE)  // 4096 frames
```

### Step 2: Initialize PMM
```c
void pmm_init(void) {
    // Calculate number of frames
    pmm.total_frames = TOTAL_FRAMES;
    pmm.used_frames = 0;
    
    // Bitmap size in bytes: (total_frames / 8) rounded up
    // Stored as uint32_t array: divide by 4 again
    pmm.bitmap_size = (pmm.total_frames / 32) + 1;
    
    // Place bitmap somewhere safe (e.g., after kernel heap)
    pmm.bitmap = (uint32_t*)PMM_BITMAP_ADDRESS;
    
    // Initially mark all frames as free (0)
    memset(pmm.bitmap, 0, pmm.bitmap_size * sizeof(uint32_t));
    
    // Mark reserved regions as used
    pmm_mark_reserved_regions();
}
```

### Step 3: Mark Reserved Regions
```c
void pmm_mark_reserved_regions(void) {
    // Reserve low memory (0 - 1MB): BIOS, bootloader, kernel
    for (uint32_t frame = 0; frame < 256; frame++) {  // 256 frames = 1MB
        pmm_set_frame(frame);
    }
    
    // Reserve kernel heap region (1MB - 2MB)
    for (uint32_t frame = 256; frame < 512; frame++) {  // 1MB of heap
        pmm_set_frame(frame);
    }
    
    // Reserve PMM bitmap itself
    uint32_t bitmap_start_frame = (uint32_t)pmm.bitmap / PAGE_SIZE;
    uint32_t bitmap_frames = (pmm.bitmap_size * sizeof(uint32_t)) / PAGE_SIZE + 1;
    for (uint32_t i = 0; i < bitmap_frames; i++) {
        pmm_set_frame(bitmap_start_frame + i);
    }
    
    // Reserve memory-mapped I/O region (15MB - 16MB)
    for (uint32_t frame = 3840; frame < 4096; frame++) {  // Last 1MB
        pmm_set_frame(frame);
    }
}
```

### Step 4: Page Allocation
```c
/**
 * alloc_page - Allocate a single physical page frame
 * 
 * Returns: Physical address of allocated page, or 0 if out of memory
 */
uint32_t alloc_page(void) {
    // Scan bitmap for first free frame (bit = 0)
    for (uint32_t i = 0; i < pmm.bitmap_size; i++) {
        if (pmm.bitmap[i] != 0xFFFFFFFF) {  // Not all bits are 1 (used)
            // Found a uint32_t with at least one free bit
            for (uint32_t j = 0; j < 32; j++) {
                uint32_t bit = 1 << j;
                if ((pmm.bitmap[i] & bit) == 0) {  // Found free bit
                    // Calculate frame number
                    uint32_t frame = i * 32 + j;
                    
                    // Mark as used
                    pmm_set_frame(frame);
                    
                    // Return physical address
                    return frame * PAGE_SIZE;
                }
            }
        }
    }
    
    // No free pages found
    return 0;
}
```

### Step 5: Page Deallocation
```c
/**
 * free_page - Free a physical page frame
 * @addr: Physical address of page to free (must be page-aligned)
 */
void free_page(uint32_t addr) {
    // Validate address is page-aligned
    if (addr % PAGE_SIZE != 0) {
        kprint("ERROR: free_page() - Address not page-aligned!\n");
        return;
    }
    
    // Calculate frame number
    uint32_t frame = addr / PAGE_SIZE;
    
    // Validate frame is in range
    if (frame >= pmm.total_frames) {
        kprint("ERROR: free_page() - Frame out of range!\n");
        return;
    }
    
    // Check if already free (double-free detection)
    if (!pmm_test_frame(frame)) {
        kprint("WARNING: free_page() - Double free detected!\n");
        return;
    }
    
    // Mark as free
    pmm_clear_frame(frame);
}
```

---

## Integration with Existing Systems

### With kmalloc (Heap Allocator)
The PMM and heap allocator serve different purposes:

```
┌─────────────────────────────────────────────────────────┐
│                     Physical RAM                        │
│                                                         │
│  ┌──────────┐  ┌──────────────┐  ┌─────────────────┐  │
│  │  Kernel  │  │   kmalloc    │  │   PMM-managed   │  │
│  │Code/Data │  │   Heap       │  │   Page Frames   │  │
│  │          │  │ (1MB-2MB)    │  │  (2MB-15MB)     │  │
│  │          │  │              │  │                 │  │
│  │          │  │ Small allocs │  │ 4KB page allocs │  │
│  │          │  │ (bytes)      │  │ (for paging)    │  │
│  └──────────┘  └──────────────┘  └─────────────────┘  │
│   Reserved       Reserved          Free Pages          │
└─────────────────────────────────────────────────────────┘

Usage patterns:
  kmalloc():  "I need 37 bytes for a string"
  alloc_page(): "I need a 4KB page for a page table"
```

**When to use which:**
- **kmalloc**: Variable-sized allocations < 4KB (strings, buffers, structs)
- **alloc_page**: Page-aligned 4KB blocks (page tables, process memory, DMA buffers)

### Reserved Memory Map
```c
#define RESERVED_LOW_MEMORY     0x00000000  // 0-1MB: Kernel, BIOS
#define HEAP_START              0x00100000  // 1MB: Start of heap
#define HEAP_END                0x00200000  // 2MB: End of heap
#define PMM_BITMAP_ADDRESS      0x00200000  // 2MB: PMM bitmap location
#define PMM_POOL_START          0x00201000  // ~2MB+: First allocatable page
#define PMM_POOL_END            0x00F00000  // 15MB: Last allocatable page
#define RESERVED_HIGH_MEMORY    0x00F00000  // 15MB+: Memory-mapped I/O
```

---

## Usage Examples

### Example 1: Allocate Page for Page Directory
```c
// Need a page for kernel's page directory
uint32_t page_dir_phys = alloc_page();
if (page_dir_phys == 0) {
    kprint("ERROR: Out of memory!\n");
    return;
}

// Clear the page (4096 bytes)
memset((void*)page_dir_phys, 0, PAGE_SIZE);

kprint("Allocated page directory at: ");
kprint_hex(page_dir_phys);
kprint("\n");
```

### Example 2: Allocate Multiple Contiguous Pages
```c
/**
 * alloc_pages - Allocate N contiguous physical pages
 * @count: Number of pages to allocate
 * 
 * Returns: Physical address of first page, or 0 if can't find contiguous block
 */
uint32_t alloc_pages(uint32_t count) {
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
```

### Example 3: Display Memory Statistics
```c
void pmm_status(void) {
    uint32_t total_kb = (pmm.total_frames * PAGE_SIZE) / 1024;
    uint32_t used_kb = (pmm.used_frames * PAGE_SIZE) / 1024;
    uint32_t free_kb = total_kb - used_kb;
    
    kprint("Physical Memory Manager Status:\n");
    kprint("  Total Memory: ");
    kprint_dec(total_kb);
    kprint(" KB\n");
    
    kprint("  Used Memory:  ");
    kprint_dec(used_kb);
    kprint(" KB (");
    kprint_dec(pmm.used_frames);
    kprint(" frames)\n");
    
    kprint("  Free Memory:  ");
    kprint_dec(free_kb);
    kprint(" KB (");
    kprint_dec(pmm.total_frames - pmm.used_frames);
    kprint(" frames)\n");
}
```

---

## Testing Strategy

### Unit Tests (15 tests in `tests/test_pmm.c`)

| Test | Category | Description |
|------|----------|-------------|
| 1 | Allocation | Single page allocation succeeds (non-zero return) |
| 2 | Validation | Returned page address is 4KB-aligned |
| 3 | Accounting | `get_used_memory()` increases by exactly 4KB after alloc |
| 4 | Freeing | `free_page()` restores used memory to pre-alloc level |
| 5 | Allocation | Allocate 3 separate pages successfully |
| 6 | Uniqueness | All 3 pages have different addresses |
| 7 | Contiguous | `alloc_pages(3)` returns a valid contiguous block |
| 8 | Cleanup | After freeing everything, free memory matches initial value |
| 9 | Double-free | Second `free_page()` on same address is rejected, counter unchanged |
| 10 | Unaligned | `free_page(0x00201001)` rejected, counter unchanged |
| 11 | Out-of-range | `free_page(0x10000000)` (256MB) rejected, counter unchanged |
| 12 | Zero-alloc | `alloc_pages(0)` returns 0 |
| 13 | Pool range | Allocated page falls within valid pool [0x201000 - 0xF00000) |
| 14 | Stress | 50 rapid alloc/free cycles complete with no memory leak |
| 15 | Statistics | `pmm_status()` displays final memory state |

### Test Coverage Summary
- **Happy paths:** single alloc, multi-alloc, contiguous alloc, free, cleanup balance
- **Error handling:** double-free detection, unaligned address rejection, out-of-range rejection, zero-count allocation
- **Integrity:** memory accounting after every operation, pool range validation, stress testing for leaks
```

---

## Common Pitfalls

### 1. **Not Reserving Kernel Memory**
```c
// WRONG: Forget to mark kernel regions as used
pmm_init();  // All frames marked free
alloc_page();  // Returns 0x00001000
// ⚠️ This overwrites your kernel code!

// CORRECT: Reserve kernel regions first
pmm_init();
pmm_mark_reserved_regions();  // Mark kernel, heap, BIOS as used
alloc_page();  // Returns safe address like 0x00201000
```

### 2. **Integer Overflow in Frame Calculation**
```c
// WRONG: If addr is large, division might overflow
uint32_t frame = addr / 4096;

// CORRECT: Use constant or explicit cast
#define PAGE_SIZE 4096
uint32_t frame = (uint32_t)(addr / PAGE_SIZE);
```

### 3. **Double Free**
```c
free_page(0x00201000);
free_page(0x00201000);  // ⚠️ Marks frame as free twice!
// Bitmap bit gets cleared, used_frames decrements twice
// Later: Same frame allocated to two different uses!

// Fix: Add double-free detection in free_page()
```

### 4. **Bitmap Size Calculation**
```c
// WRONG: Not enough space for all frames
bitmap_size = total_frames / 32;  // Missing remainder!

// CORRECT: Round up
bitmap_size = (total_frames + 31) / 32;  // Or: (total_frames / 32) + 1
```

---

## Next Phase Preview: Paging (MMU)

Once PMM is working, Phase 10 will use these page frames to build page tables:

```c
// Phase 10 example: Create a page table
uint32_t page_table_phys = alloc_page();  // ← Uses PMM from Phase 9
page_table_entry_t* page_table = (page_table_entry_t*)page_table_phys;

// Map virtual address 0x00000000 to physical 0x00000000 (identity map)
page_table[0] = alloc_page() | PAGE_PRESENT | PAGE_WRITE;
page_table[1] = alloc_page() | PAGE_PRESENT | PAGE_WRITE;
// ...
```

The PMM provides the "raw materials" (physical page frames) that the paging system organizes into virtual address spaces.

---

## Summary

**Physical Memory Manager (PMM):**
- Manages physical RAM at 4KB page granularity
- Bitmap allocator: simple, efficient, constant overhead
- Tracks free/used page frames
- Foundation for virtual memory (Phase 10)

**Key Functions:**
- `pmm_init()` - Initialize allocator and bitmap
- `alloc_page()` - Allocate a 4KB physical page
- `free_page()` - Free a physical page
- `pmm_status()` - Display memory statistics

**Integration:**
- Coexists with kmalloc (different use cases)
- Reserves kernel/heap/BIOS regions
- Provides frames for future page tables

**Ready for:** Phase 10 (Paging/Virtual Memory) 🚀
