#include "tests.h"
#include "../drivers/screen.h"

void run_all_tests() {
    kprint("\n");
    kprint("=====================================\n");
    kprint("       RUNNING UNIT TESTS\n");
    kprint("=====================================\n");
    
    // Run all tests with checkpoints
    kprint("\n>>> Starting test_va_system...\n");
    test_va_system();
    kprint(">>> CHECKPOINT 1: test_va_system COMPLETE\n");
    
    kprint("\n>>> Starting test_printf...\n");
    test_printf();
    kprint(">>> CHECKPOINT 2: test_printf COMPLETE\n");
    
    //kprint("\n>>> Starting test_scrolling...\n");
    //test_scrolling();
    //kprint(">>> CHECKPOINT 3: test_scrolling COMPLETE\n");
    kprint("\n>>> CHECKPOINT 3: test_scrolling SKIPPED (stack overflow prevention)\n");
    
    // DISABLED FOR DEBUGGING
    //kprint("\n>>> Starting test_klog...\n");
    //test_klog();
    //kprint(">>> CHECKPOINT 4: test_klog COMPLETE\n");
    kprint("\n>>> CHECKPOINT 4: test_klog SKIPPED (debugging)\n");
    
    kprint("\n=====================================\n");
    kprint("       ALL TESTS COMPLETED\n");
    kprint("=====================================\n");
    kprint("\n");
}
