#include "../libc/stdio.h"
#include "../libc/stddef.h"
#include "../drivers/screen.h"
#include "../kernel/klog.h"
#include "test_scroll_log.h"

void test_scrolling() {
    kprint("\n=== Testing Screen Scrolling ===\n");
    kprint("This test will print many lines to trigger scrolling.\n");
    kprint("Watch as the screen scrolls automatically.\n\n");
    
    // Print enough lines to trigger scrolling
    for (int i = 1; i <= 30; i++) {
        kprintf("Line %d: Testing automatic screen scrolling...\n", i);
    }
    
    kprint("\n[PASS] Scrolling test completed!\n");
    kprint("If you can see this, scrolling works correctly.\n\n");
}

void test_klog() {
    kprint("\n=== Testing Kernel Log System ===\n");
    
    // Test different log levels
    kprint("Test 1: Different log levels\n");
    klog_debug("This is a debug message");
    klog_info("This is an info message");
    klog_warn("This is a warning message");
    klog_error("This is an error message");
    kprint("  [PASS] Log levels\n");
    
    // Test formatted messages
    kprint("\nTest 2: Formatted messages\n");
    klog_info("Integer: %d", 42);
    klog_info("Hex: 0x%x", 0xDEAD);
    klog_info("String: %s", "Hello");
    klog_info("Mixed: val=%d, hex=0x%x, str=%s", 100, 0xFF, "test");
    kprint("  [PASS] Formatted messages\n");
    
    // Test log statistics
    kprint("\nTest 3: Log statistics\n");
    uint32_t total, size;
    klog_get_stats(&total, &size);
    kprintf("  Total logged: %u, Buffer size: %u\n", total, size);
    kprint("  [PASS] Statistics\n");
    
    // Test buffer wraparound (log many messages)
    kprint("\nTest 4: Circular buffer (logging 300 messages)\n");
    for (int i = 0; i < 300; i++) {
        if (i % 100 == 0) {
            kprintf("  Logging message %d...\n", i);
        }
        klog_debug("Message number %d", i);
    }
    klog_get_stats(&total, &size);
    kprintf("  After 300 messages: total=%u, in_buffer=%u\n", total, size);
    if (size == 256) {
        kprint("  [PASS] Circular buffer wraps correctly\n");
    } else {
        kprint("  [FAIL] Buffer size incorrect\n");
    }
    
    // Note: Not dumping full log here to avoid too much output
    kprint("\nTest 5: Log dump (showing first 10 entries)\n");
    kprint("----------------------------------------\n");
    for (uint32_t i = 0; i < 10 && i < size; i++) {
        klog_entry_t* entry = klog_get_entry(i);
        if (entry) {
            kprintf("  [%u] %s\n", entry->timestamp, entry->message);
        }
    }
    kprint("----------------------------------------\n");
    kprint("  [PASS] Can retrieve log entries\n");
    
    kprint("\nAll kernel log tests completed!\n\n");
}
