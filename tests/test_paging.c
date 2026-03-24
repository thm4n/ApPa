/**
 * test_paging.c - Paging subsystem unit tests
 *
 * Verifies identity mapping correctness, boundary conditions,
 * dynamic map/unmap operations, and unmapped address handling.
 * Runs after paging_init() has been called in kernel_main().
 */

#include "test_paging.h"
#include "../drivers/screen.h"
#include "../kernel/paging.h"
#include "../kernel/pmm.h"

void test_paging(void) {
    kprint("=== Testing Paging Subsystem ===\n");

    int passed = 0;
    int failed = 0;

    // ── Test 1: Identity map kernel code region ──
    kprint("\nTest 1: Identity map kernel (0x1000)...\n");
    {
        uint32_t result = paging_translate(0x1000);
        if (result == 0x1000) {
            kprint("  [PASS] translate(0x1000) == 0x1000\n");
            passed++;
        } else {
            kprint("  [FAIL] Expected 0x1000, got 0x");
            kprint_hex(result);
            kprint("\n");
            failed++;
        }
    }

    // ── Test 2: Identity map VGA framebuffer ──
    kprint("\nTest 2: Identity map VGA (0xB8000)...\n");
    {
        uint32_t result = paging_translate(0xB8000);
        if (result == 0xB8000) {
            kprint("  [PASS] translate(0xB8000) == 0xB8000\n");
            passed++;
        } else {
            kprint("  [FAIL] Expected 0xB8000, got 0x");
            kprint_hex(result);
            kprint("\n");
            failed++;
        }
    }

    // ── Test 3: Identity map kernel heap ──
    kprint("\nTest 3: Identity map heap (0x100000)...\n");
    {
        uint32_t result = paging_translate(0x100000);
        if (result == 0x100000) {
            kprint("  [PASS] translate(0x100000) == 0x100000\n");
            passed++;
        } else {
            kprint("  [FAIL] Expected 0x100000, got 0x");
            kprint_hex(result);
            kprint("\n");
            failed++;
        }
    }

    // ── Test 4: Identity map PMM pool ──
    kprint("\nTest 4: Identity map PMM pool (0x201000)...\n");
    {
        uint32_t result = paging_translate(0x201000);
        if (result == 0x201000) {
            kprint("  [PASS] translate(0x201000) == 0x201000\n");
            passed++;
        } else {
            kprint("  [FAIL] Expected 0x201000, got 0x");
            kprint_hex(result);
            kprint("\n");
            failed++;
        }
    }

    // ── Test 5: Boundary — last mapped page (top of 16 MB) ──
    kprint("\nTest 5: Boundary last page (0xFFF000)...\n");
    {
        uint32_t result = paging_translate(0xFFF000);
        if (result == 0xFFF000) {
            kprint("  [PASS] translate(0xFFF000) == 0xFFF000\n");
            passed++;
        } else {
            kprint("  [FAIL] Expected 0xFFF000, got 0x");
            kprint_hex(result);
            kprint("\n");
            failed++;
        }
    }

    // ── Test 6: Unmapped address returns 0 ──
    kprint("\nTest 6: Unmapped addr (0x1000000) returns 0...\n");
    {
        uint32_t result = paging_translate(0x1000000);  // 16 MB — not mapped
        if (result == 0) {
            kprint("  [PASS] translate(0x1000000) == 0 (not mapped)\n");
            passed++;
        } else {
            kprint("  [FAIL] Expected 0, got 0x");
            kprint_hex(result);
            kprint("\n");
            failed++;
        }
    }

    // ── Test 7: Map a new page dynamically ──
    kprint("\nTest 7: Dynamic map (0x2000000 -> new frame)...\n");
    {
        // Allocate a physical frame to map
        uint32_t frame = alloc_page();
        if (frame == 0) {
            kprint("  [SKIP] Could not allocate frame\n");
        } else {
            // Map virtual 0x2000000 (32 MB) to the allocated frame
            paging_map_page(0x2000000, frame, PAGE_PRESENT | PAGE_WRITABLE);

            uint32_t result = paging_translate(0x2000000);
            if (result == frame) {
                kprint("  [PASS] translate(0x2000000) == allocated frame 0x");
                kprint_hex(frame);
                kprint("\n");
                passed++;
            } else {
                kprint("  [FAIL] Expected 0x");
                kprint_hex(frame);
                kprint(", got 0x");
                kprint_hex(result);
                kprint("\n");
                failed++;
            }

            // ── Test 8: Unmap the page ──
            kprint("\nTest 8: Unmap (0x2000000)...\n");
            paging_unmap_page(0x2000000);

            result = paging_translate(0x2000000);
            if (result == 0) {
                kprint("  [PASS] translate(0x2000000) == 0 after unmap\n");
                passed++;
            } else {
                kprint("  [FAIL] Expected 0, got 0x");
                kprint_hex(result);
                kprint("\n");
                failed++;
            }

            // Free the physical frame we allocated
            free_page(frame);
        }
    }

    // ── Summary ──
    kprint("\nPaging tests: ");
    kprint_uint(passed);
    kprint(" passed, ");
    kprint_uint(failed);
    kprint(" failed\n");

    if (failed == 0) {
        kprint("[ALL PAGING TESTS PASSED]\n");
    }
}
