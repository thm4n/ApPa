#include "tests.h"
#include "../drivers/screen.h"

void run_all_tests() {
    kprint("\n");
    kprint("=====================================\n");
    kprint("       RUNNING UNIT TESTS\n");
    kprint("=====================================\n");
    
    // Run all tests
    test_va_system();
    test_printf();
    test_scrolling();
    test_klog();
    
    kprint("=====================================\n");
    kprint("       ALL TESTS COMPLETED\n");
    kprint("=====================================\n");
    kprint("\n");
}
