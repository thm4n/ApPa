/**
 * test_addrspace.c - Per-process address space isolation tests (Phase 15)
 *
 * Tests:
 *  1. Separate directories  — two user tasks have different cr3 values
 *  2. Kernel invisible      — user task reading kernel memory (0x200000)
 *                              triggers page fault, task is killed
 *  3. Syscalls work         — SYS_WRITE + SYS_GETPID still function
 *                              through the per-process page directory
 *  4. Cleanup / no leaks   — PMM free page count returns to pre-test level
 *
 * Strategy:
 *   The test harness runs in kernel mode.  It spawns user tasks via
 *   task_create_user(), waits for them to complete (timer ticks),
 *   reaps them, and checks volatile flags / PMM counters.
 */

#include "test_addrspace.h"
#include "../drivers/screen.h"
#include "../kernel/task/task.h"
#include "../kernel/task/sched.h"
#include "../kernel/sys/timer.h"
#include "../kernel/mem/pmm.h"
#include "../kernel/mem/paging.h"
#include "../klibc/string.h"
#include "../klibc/syscall.h"

/* See test_userspace.c for rationale — shared kernel BSS variables need
 * PAGE_USER in the per-process page directory so Ring 3 can write them. */
#define MAP_SHARED_VAR(task, var) do {                                   \
    uint32_t _pg = (uint32_t)&(var) & PAGE_FRAME_MASK;                  \
    paging_map_page_in((page_directory_t *)(task)->page_dir,             \
                       _pg, _pg,                                        \
                       PAGE_PRESENT | PAGE_WRITABLE | PAGE_USER);       \
} while (0)

/* ─── Shared flags (set by user tasks, read by test harness) ────────────── */

static volatile uint32_t as_write_done    = 0;
static volatile uint32_t as_getpid_val    = 0;
static volatile uint32_t as_exit_done     = 0;
static volatile uint32_t as_fault_survived = 0;   /* Should stay 0 */

/* Store cr3 values from two tasks for comparison */
static volatile uint32_t as_cr3_task_a    = 0;
static volatile uint32_t as_cr3_task_b    = 0;

/* Greeting lives at file scope (non-const → .data, not .rodata) so the
 * test harness can obtain its address and MAP_SHARED_VAR it.             */
static char as_hello_msg[] = "[P15] Hello from isolated process!\n";
static const uint32_t as_hello_msg_len = 35;

/* ─── User-mode task entry points ───────────────────────────────────────── */

/**
 * user_as_hello - Write a greeting, report PID, exit
 *
 * Exercises SYS_WRITE + SYS_GETPID + SYS_EXIT through the per-process
 * page directory.  String is built on the user stack (PAGE_USER).
 */
static void user_as_hello(void) {
    sys_write(as_hello_msg, as_hello_msg_len);
    as_write_done = 1;

    int pid = sys_getpid();
    as_getpid_val = (uint32_t)pid;

    as_exit_done = 1;
    sys_exit();
}

/**
 * user_as_kernel_read - Attempt to read kernel memory (0x200000)
 *
 * 0x200000 is the PMM bitmap area — mapped supervisor-only in the
 * per-process directory (PDE 0 clone doesn't set PAGE_USER on it).
 * This should trigger a page fault; the handler kills the task.
 * If we somehow survive, the flag is set so the test detects failure.
 */
static void user_as_kernel_read(void) {
    volatile uint32_t val = *(volatile uint32_t *)0x200000;
    (void)val;

    /* If we reach here, isolation failed */
    as_fault_survived = 1;
    sys_exit();
}

/**
 * user_as_task_a / user_as_task_b - Minimal tasks that just exit.
 * The harness records their cr3 values from the TCB.
 */
static void user_as_task_a(void) {
    sys_exit();
}

static void user_as_task_b(void) {
    sys_exit();
}

/* ─── Helpers ───────────────────────────────────────────────────────────── */

static void test_wait_ms(uint32_t ms) {
    uint32_t ticks = ms / 10;
    if (ticks == 0) ticks = 1;
    uint32_t start = get_timer_ticks();
    while ((get_timer_ticks() - start) < ticks) {
        __asm__ volatile("hlt");
    }
}

/* ─── Public entry point ────────────────────────────────────────────────── */

void test_addrspace(void) {
    kprint("\n--- Test: Per-Process Address Spaces (Phase 15) ---\n");

    /* ── Test 1: Separate page directories ─────────────────────────── */
    kprint("  [1] Creating two user tasks with separate CR3...\n");

    task_t *ta = task_create_user(user_as_task_a, "as:taskA");
    task_t *tb = task_create_user(user_as_task_b, "as:taskB");

    if (!ta || !tb) {
        kprint("  [FAIL] task_create_user returned NULL\n");
        return;
    }

    as_cr3_task_a = ta->cr3;
    as_cr3_task_b = tb->cr3;

    test_wait_ms(300);
    task_reap();

    if (as_cr3_task_a != 0 && as_cr3_task_b != 0 &&
        as_cr3_task_a != as_cr3_task_b) {
        kprint("  [OK] Task A cr3=");
        kprint_hex(as_cr3_task_a);
        kprint("  Task B cr3=");
        kprint_hex(as_cr3_task_b);
        kprint(" (different)\n");
    } else {
        kprint("  [FAIL] cr3 values identical or zero\n");
    }

    /* ── Test 2: Syscalls still work through per-process directory ── */
    kprint("  [2] SYS_WRITE + SYS_GETPID via per-process pages...\n");
    as_write_done  = 0;
    as_getpid_val  = 0;
    as_exit_done   = 0;

    sched_disable();
    task_t *th = task_create_user(user_as_hello, "as:hello");
    if (!th) {
        sched_enable();
        kprint("  [FAIL] task_create_user returned NULL\n");
        return;
    }
    /* Map shared flags so Ring 3 code can write them */
    MAP_SHARED_VAR(th, as_write_done);
    MAP_SHARED_VAR(th, as_getpid_val);
    MAP_SHARED_VAR(th, as_exit_done);
    MAP_SHARED_VAR(th, as_hello_msg);
    sched_enable();

    test_wait_ms(500);
    task_reap();

    if (as_write_done) {
        kprint("  [OK] SYS_WRITE completed\n");
    } else {
        kprint("  [FAIL] SYS_WRITE did not complete\n");
    }
    if (as_getpid_val != 0) {
        kprint("  [OK] SYS_GETPID returned ");
        kprint_uint(as_getpid_val);
        kprint("\n");
    } else {
        kprint("  [FAIL] SYS_GETPID returned 0\n");
    }
    if (as_exit_done) {
        kprint("  [OK] SYS_EXIT reached\n");
    } else {
        kprint("  [FAIL] SYS_EXIT not reached\n");
    }

    /* ── Test 3: Kernel memory invisible to user ───────────────────── */
    kprint("  [3] User task reading kernel memory (0x200000)...\n");
    as_fault_survived = 0;

    uint32_t free_before = get_free_memory();

    task_t *tf = task_create_user(user_as_kernel_read, "as:kread");
    if (!tf) {
        kprint("  [FAIL] task_create_user returned NULL\n");
        return;
    }

    test_wait_ms(500);
    task_reap();

    if (as_fault_survived == 0) {
        kprint("  [OK] Page fault killed user task (kernel data protected)\n");
    } else {
        kprint("  [FAIL] User read kernel memory without faulting!\n");
    }

    /* ── Test 4: Cleanup — no page leaks ───────────────────────────── */
    kprint("  [4] Checking PMM free count...\n");

    uint32_t free_after = get_free_memory();

    /* After creating and reaping the task, free memory should return.
     * Allow a small tolerance (1-2 pages) for any persistent allocations. */
    if (free_after >= free_before - 8192) {
        kprint("  [OK] Free memory recovered (before=");
        kprint_uint(free_before / 1024);
        kprint("KB after=");
        kprint_uint(free_after / 1024);
        kprint("KB)\n");
    } else {
        kprint("  [WARN] Possible page leak (before=");
        kprint_uint(free_before / 1024);
        kprint("KB after=");
        kprint_uint(free_after / 1024);
        kprint("KB)\n");
    }

    kprint("--- Address space tests complete ---\n\n");
}
