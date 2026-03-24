/**
 * test_pmm.c - Physical Memory Manager unit tests
 */

#include "test_pmm.h"
#include "../drivers/screen.h"
#include "../kernel/pmm.h"
#include "../libc/string.h"

void test_pmm(void) {
    kprint("=== Testing Physical Memory Manager ===\n");
    
    uint32_t initial_free = get_free_memory();
    uint32_t initial_used = get_used_memory();
    
    // Test 1: Allocate single page
    kprint("\nTest 1: Single page allocation...\n");
    uint32_t page1 = alloc_page();
    if (page1 == 0) {
        kprint("  [FAIL] Could not allocate page\n");
        return;
    }
    kprint("  [PASS] Allocated page at: 0x");
    kprint_hex(page1);
    kprint("\n");
    
    // Test 2: Verify page alignment
    kprint("\nTest 2: Page alignment check...\n");
    if (page1 % PAGE_SIZE != 0) {
        kprint("  [FAIL] Page not aligned (offset: ");
        kprint_uint(page1 % PAGE_SIZE);
        kprint(")\n");
        return;
    }
    kprint("  [PASS] Page is properly aligned (4KB boundary)\n");
    
    // Test 3: Verify memory accounting
    kprint("\nTest 3: Memory accounting...\n");
    uint32_t used_after_alloc = get_used_memory();
    if (used_after_alloc != initial_used + PAGE_SIZE) {
        kprint("  [FAIL] Memory accounting incorrect\n");
        kprint("         Expected: ");
        kprint_uint(initial_used + PAGE_SIZE);
        kprint(" bytes, Got: ");
        kprint_uint(used_after_alloc);
        kprint(" bytes\n");
        return;
    }
    kprint("  [PASS] Memory accounting correct (+4KB used)\n");
    
    // Test 4: Free the page
    kprint("\nTest 4: Free page...\n");
    free_page(page1);
    uint32_t used_after_free = get_used_memory();
    if (used_after_free != initial_used) {
        kprint("  [FAIL] Memory not freed properly\n");
        return;
    }
    kprint("  [PASS] Page freed successfully\n");
    
    // Test 5: Allocate multiple pages
    kprint("\nTest 5: Multiple page allocation...\n");
    uint32_t page2 = alloc_page();
    uint32_t page3 = alloc_page();
    uint32_t page4 = alloc_page();
    
    if (!page2 || !page3 || !page4) {
        kprint("  [FAIL] Could not allocate 3 pages\n");
        return;
    }
    
    kprint("  [PASS] Allocated 3 pages:\n");
    kprint("         Page 2: 0x");
    kprint_hex(page2);
    kprint("\n         Page 3: 0x");
    kprint_hex(page3);
    kprint("\n         Page 4: 0x");
    kprint_hex(page4);
    kprint("\n");
    
    // Test 6: Verify pages are different
    kprint("\nTest 6: Unique page addresses...\n");
    if (page2 == page3 || page2 == page4 || page3 == page4) {
        kprint("  [FAIL] Duplicate page addresses detected!\n");
        return;
    }
    kprint("  [PASS] All pages have unique addresses\n");
    
    // Test 7: Allocate contiguous pages
    kprint("\nTest 7: Contiguous page allocation (3 pages)...\n");
    uint32_t contig_start = alloc_pages(3);
    if (contig_start == 0) {
        kprint("  [FAIL] Could not allocate 3 contiguous pages\n");
    } else {
        kprint("  [PASS] Allocated 3 contiguous pages at: 0x");
        kprint_hex(contig_start);
        kprint("\n");
        
        // Free contiguous pages
        free_page(contig_start);
        free_page(contig_start + PAGE_SIZE);
        free_page(contig_start + (2 * PAGE_SIZE));
    }
    
    // Test 8: Clean up remaining allocations
    kprint("\nTest 8: Cleanup...\n");
    free_page(page2);
    free_page(page3);
    free_page(page4);
    
    uint32_t final_free = get_free_memory();
    if (final_free != initial_free) {
        kprint("  [WARN] Free memory mismatch after cleanup\n");
        kprint("         Initial: ");
        kprint_uint(initial_free);
        kprint(" bytes, Final: ");
        kprint_uint(final_free);
        kprint(" bytes\n");
    } else {
        kprint("  [PASS] All allocated memory freed\n");
    }
    
    // Test 9: Double-free detection
    kprint("\nTest 9: Double-free detection...\n");
    uint32_t df_page = alloc_page();
    if (df_page == 0) {
        kprint("  [FAIL] Could not allocate page for double-free test\n");
        return;
    }
    free_page(df_page);
    uint32_t used_before_double_free = get_used_memory();
    kprint("  (Expecting WARNING message below)\n");
    free_page(df_page);  // Should print warning and NOT decrement used count
    uint32_t used_after_double_free = get_used_memory();
    if (used_after_double_free == used_before_double_free) {
        kprint("  [PASS] Double-free safely rejected, used_frames unchanged\n");
    } else {
        kprint("  [FAIL] Double-free corrupted used_frames counter\n");
    }
    
    // Test 10: Unaligned address to free_page
    kprint("\nTest 10: Unaligned address rejection...\n");
    uint32_t used_before_unaligned = get_used_memory();
    kprint("  (Expecting ERROR message below)\n");
    free_page(0x00201001);  // Not 4KB-aligned
    uint32_t used_after_unaligned = get_used_memory();
    if (used_after_unaligned == used_before_unaligned) {
        kprint("  [PASS] Unaligned address safely rejected\n");
    } else {
        kprint("  [FAIL] Unaligned free corrupted used_frames counter\n");
    }
    
    // Test 11: Out-of-range address to free_page
    kprint("\nTest 11: Out-of-range address rejection...\n");
    uint32_t used_before_oor = get_used_memory();
    kprint("  (Expecting ERROR message below)\n");
    free_page(0x10000000);  // 256MB — well beyond 16MB total
    uint32_t used_after_oor = get_used_memory();
    if (used_after_oor == used_before_oor) {
        kprint("  [PASS] Out-of-range address safely rejected\n");
    } else {
        kprint("  [FAIL] Out-of-range free corrupted used_frames counter\n");
    }
    
    // Test 12: Allocate zero pages
    kprint("\nTest 12: alloc_pages(0) returns 0...\n");
    uint32_t zero_alloc = alloc_pages(0);
    if (zero_alloc == 0) {
        kprint("  [PASS] alloc_pages(0) correctly returned 0\n");
    } else {
        kprint("  [FAIL] alloc_pages(0) returned non-zero: 0x");
        kprint_hex(zero_alloc);
        kprint("\n");
    }
    
    // Test 13: Allocate page in valid pool range
    kprint("\nTest 13: Allocated page within valid pool range...\n");
    uint32_t pool_page = alloc_page();
    if (pool_page >= PMM_POOL_START && pool_page < PMM_POOL_END) {
        kprint("  [PASS] Page 0x");
        kprint_hex(pool_page);
        kprint(" is within pool range [0x");
        kprint_hex(PMM_POOL_START);
        kprint(" - 0x");
        kprint_hex(PMM_POOL_END);
        kprint(")\n");
    } else {
        kprint("  [WARN] Page 0x");
        kprint_hex(pool_page);
        kprint(" is outside expected pool range\n");
    }
    free_page(pool_page);
    
    // Test 14: Stress test — rapid alloc/free cycles
    kprint("\nTest 14: Rapid alloc/free stress test (50 cycles)...\n");
    uint32_t stress_used_before = get_used_memory();
    uint32_t stress_ok = 1;
    for (uint32_t i = 0; i < 50; i++) {
        uint32_t p = alloc_page();
        if (p == 0) {
            kprint("  [FAIL] Allocation failed at cycle ");
            kprint_uint(i);
            kprint("\n");
            stress_ok = 0;
            break;
        }
        free_page(p);
    }
    uint32_t stress_used_after = get_used_memory();
    if (stress_ok && stress_used_after == stress_used_before) {
        kprint("  [PASS] 50 alloc/free cycles completed, memory balanced\n");
    } else if (stress_ok) {
        kprint("  [FAIL] Memory leak detected after stress test\n");
    }
    
    // Test 15: Display final statistics
    kprint("\nTest 15: Final PMM statistics:\n");
    pmm_status();
    
    kprint("\n=== PMM Tests Complete ===\n");
}
