/**
 * test_userspace.c - Ring 3 userspace tests
 *
 * Tests:
 *  1. User task creation   — task_create_user() returns a valid TCB
 *  2. SYS_WRITE            — user task prints via INT 0x80
 *  3. SYS_GETPID           — user task reads its own task ID
 *  4. SYS_YIELD            — user task yields cooperatively
 *  5. SYS_EXIT             — user task terminates cleanly
 *  6. GPF from user mode   — executing CLI in Ring 3 triggers #GP,
 *                             kernel kills only that task
 *
 * Strategy:
 *   Kernel-side test harness creates user tasks with task_create_user(),
 *   waits a few hundred ms (via timer ticks), then checks shared
 *   volatile flags that the user tasks set through syscalls.
 *
 * NOTE: Because we don't yet have separate address spaces, user-mode
 * code can still read/write kernel memory (the identity-mapped pages
 * are supervisor-only in the page tables, but the PDE/PTE U/S bits
 * for the first 16 MB are currently 0).  For the SYS_WRITE test to
 * work the string must live in user-accessible memory.  We handle
 * this by having the user entry function copy the message onto its
 * own (user-mapped) stack, which is mapped with PAGE_USER.
 */

#include "test_userspace.h"
#include "../drivers/screen.h"
#include "../kernel/task/task.h"
#include "../kernel/task/sched.h"
#include "../kernel/sys/timer.h"
#include "../kernel/mem/paging.h"
#include "../klibc/string.h"
#include "../klibc/syscall.h"

/*
 * MAP_SHARED_VAR - Map the page containing a kernel-resident variable
 * as user-accessible in a task's per-process page directory.
 *
 * With Phase 15 per-process address spaces, kernel BSS/data pages are
 * supervisor-only.  The test's static volatile flags live in kernel BSS,
 * so user tasks page-fault when they try to write them.  This macro
 * marks the specific page(s) as PAGE_USER in the clone so the old
 * shared-variable test design keeps working.
 */
#define MAP_SHARED_VAR(task, var) do {                                   \
    uint32_t _pg = (uint32_t)&(var) & PAGE_FRAME_MASK;                  \
    paging_map_page_in((page_directory_t *)(task)->page_dir,             \
                       _pg, _pg,                                        \
                       PAGE_PRESENT | PAGE_WRITABLE | PAGE_USER);       \
} while (0)

/* ─── Shared flags (set by user tasks, read by test harness) ────────────── */

static volatile uint32_t user_write_done   = 0;  /* Set after SYS_WRITE   */
static volatile uint32_t user_getpid_val   = 0;  /* Task ID from GETPID   */
static volatile uint32_t user_exit_done    = 0;  /* Set just before EXIT  */
static volatile uint32_t user_gpf_survived = 0;  /* Should stay 0 (killed)*/
static volatile uint32_t user_yield_count  = 0;  /* SYS_YIELD invocations */

/* ─── User-mode task entry points ───────────────────────────────────────── */

/**
 * user_task_hello - Write a greeting via SYS_WRITE and exit
 *
 * Copies the message onto the user stack (which is PAGE_USER mapped),
 * then passes that stack-local pointer to SYS_WRITE so pointer
 * validation against USER_SPACE_START succeeds.
 */
static void user_task_hello(void) {
    /* Build the message on the user stack so the pointer is in
     * user-accessible memory (the user stack page is mapped with
     * PAGE_USER). */
    char msg[] = "Hello from Ring 3!\n";

    sys_write(msg, 19);
    user_write_done = 1;

    int pid = sys_getpid();
    user_getpid_val = (uint32_t)pid;

    user_exit_done = 1;
    sys_exit();
}

/**
 * user_task_yield - Yield the CPU a few times and exit
 */
static void user_task_yield(void) {
    for (int i = 0; i < 5; i++) {
        sys_yield();
        user_yield_count++;
    }
    sys_exit();
}

/**
 * user_task_gpf - Execute a privileged instruction (CLI) in Ring 3
 *
 * This should trigger a #GP exception.  The kernel's GPF handler
 * should kill this task and NOT halt the system.  The flag
 * user_gpf_survived should remain 0 because we never reach the
 * line after the CLI.
 */
static void user_task_gpf(void) {
    /* This will cause #GP in Ring 3 */
    __asm__ volatile("cli");

    /* If we somehow get here, the GPF handler didn't work */
    user_gpf_survived = 1;
    sys_exit();
}

/* ─── Helpers ───────────────────────────────────────────────────────────── */

/** Busy-wait for approximately ms milliseconds (at 100 Hz PIT). */
static void test_wait_ms(uint32_t ms) {
    uint32_t ticks = ms / 10;
    if (ticks == 0) ticks = 1;
    uint32_t start = get_timer_ticks();
    while ((get_timer_ticks() - start) < ticks) {
        __asm__ volatile("hlt");   /* Save power while waiting */
    }
}

/* ─── Public test entry point ───────────────────────────────────────────── */

void test_userspace(void) {
    kprint("\n--- Test: Userspace (Ring 3) ---\n");

    /* ── Test 1: Create a user task and verify SYS_WRITE + GETPID ──── */
    kprint("  [1] Creating user task 'hello'...\n");
    user_write_done  = 0;
    user_getpid_val  = 0;
    user_exit_done   = 0;

    /* Disable scheduling so the new task can't run before we
     * finish mapping the shared variable pages in its address space. */
    sched_disable();
    task_t *t1 = task_create_user(user_task_hello, "u:hello");
    if (!t1) {
        sched_enable();
        kprint("  [FAIL] task_create_user returned NULL\n");
        return;
    }
    /* Map shared flags so Ring 3 code can write them */
    MAP_SHARED_VAR(t1, user_write_done);
    MAP_SHARED_VAR(t1, user_getpid_val);
    MAP_SHARED_VAR(t1, user_exit_done);
    sched_enable();
    kprint("  [OK] task_create_user returned TCB id=");
    kprint_uint(t1->id);
    kprint("\n");

    /* Wait for the user task to run and exit */
    test_wait_ms(500);
    task_reap();

    if (user_write_done) {
        kprint("  [OK] SYS_WRITE completed\n");
    } else {
        kprint("  [FAIL] SYS_WRITE did not complete\n");
    }

    if (user_getpid_val != 0) {
        kprint("  [OK] SYS_GETPID returned ");
        kprint_uint(user_getpid_val);
        kprint("\n");
    } else {
        kprint("  [FAIL] SYS_GETPID returned 0\n");
    }

    if (user_exit_done) {
        kprint("  [OK] SYS_EXIT reached\n");
    } else {
        kprint("  [FAIL] SYS_EXIT not reached\n");
    }

    /* ── Test 2: SYS_YIELD ─────────────────────────────────────────── */
    kprint("  [2] Creating user task 'yield'...\n");
    user_yield_count = 0;

    sched_disable();
    task_t *t2 = task_create_user(user_task_yield, "u:yield");
    if (!t2) {
        sched_enable();
        kprint("  [FAIL] task_create_user returned NULL\n");
        return;
    }
    MAP_SHARED_VAR(t2, user_yield_count);
    sched_enable();

    test_wait_ms(500);
    task_reap();

    if (user_yield_count >= 3) {
        kprint("  [OK] SYS_YIELD invoked ");
        kprint_uint(user_yield_count);
        kprint(" times\n");
    } else {
        kprint("  [FAIL] SYS_YIELD count=");
        kprint_uint(user_yield_count);
        kprint(" (expected >= 3)\n");
    }

    /* ── Test 3: GPF from user mode ────────────────────────────────── */
    kprint("  [3] Creating user task 'gpf' (will trigger #GP)...\n");
    user_gpf_survived = 0;

    task_t *t3 = task_create_user(user_task_gpf, "u:gpf");
    if (!t3) {
        kprint("  [FAIL] task_create_user returned NULL\n");
        return;
    }

    test_wait_ms(500);
    task_reap();

    if (user_gpf_survived == 0) {
        kprint("  [OK] GPF killed user task (system survived)\n");
    } else {
        kprint("  [FAIL] user_gpf_survived == 1 (GPF not caught)\n");
    }

    kprint("--- Userspace tests complete ---\n\n");
}
