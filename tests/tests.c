#include "tests.h"
#include "../drivers/screen.h"

void run_all_tests() {
    kprint("\n");
    kprint("=====================================\n");
    kprint("       RUNNING UNIT TESTS\n");
    kprint("=====================================\n");
    
    // Test 1: va_list system
    kprint("\n>>> Starting test_va_system...\n");
    test_va_system();
    kprint(">>> CHECKPOINT 1: test_va_system COMPLETE\n");
    
    // Test 2: printf
    kprint("\n>>> Starting test_printf...\n");
    test_printf();
    kprint(">>> CHECKPOINT 2: test_printf COMPLETE\n");
    
    // Test 3: kernel logging (disabled - debugging)
    //kprint("\n>>> Starting test_klog...\n");
    //test_klog();
    //kprint(">>> CHECKPOINT 3: test_klog COMPLETE\n");
    kprint("\n>>> CHECKPOINT 3: test_klog SKIPPED (debugging)\n");
    
    // Test 4: physical memory manager
    kprint("\n>>> Starting test_pmm...\n");
    test_pmm();
    kprint(">>> CHECKPOINT 4: test_pmm COMPLETE\n");
    // Test 5: paging subsystem
    kprint("\n>>> Starting test_paging...\n");
    test_paging();
    kprint(">>> CHECKPOINT 5: test_paging COMPLETE\n");	

    // Test 6: ATA PIO driver
    kprint("\n>>> Starting test_ata...\n");
    test_ata();
    kprint(">>> CHECKPOINT 6: test_ata COMPLETE\n");

    // Test 7: SimpleFS filesystem
    kprint("\n>>> Starting test_fs...\n");
    test_fs();
    kprint(">>> CHECKPOINT 7: test_fs COMPLETE\n");

	kprint("\n=====================================\n");
	kprint("       ALL TESTS COMPLETED\n");
	kprint("=====================================\n");
	kprint("\n");

	// Note: Userspace tests run AFTER sched_enable() — see kernel_main.c
}