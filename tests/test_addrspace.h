/**
 * test_addrspace.h - Per-process address space isolation tests (Phase 15)
 *
 * Tests:
 *  1. Separate page directories   — two user tasks have different cr3 values
 *  2. Kernel memory invisible     — user task reading kernel data page-faults
 *  3. Syscalls still work         — SYS_WRITE / SYS_GETPID via INT 0x80
 *  4. Cleanup / no page leaks     — PMM free count returns after reap
 *
 * Must be called after syscall_init() + sched_enable().
 */

#ifndef TEST_ADDRSPACE_H
#define TEST_ADDRSPACE_H

/**
 * test_addrspace - Run all per-process address space tests
 */
void test_addrspace(void);

#endif /* TEST_ADDRSPACE_H */
