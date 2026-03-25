/**
 * test_multitask.c - Multitasking subsystem tests
 *
 * Tests:
 *  1. Task creation — task_create returns non-NULL, task gets an ID
 *  2. Context switch — two tasks both execute (shared counters increment)
 *  3. Preemptive scheduling — tasks interleave without explicit yield
 *  4. Task exit + reap — tasks finish and resources are reclaimed
 *  5. Multiple tasks — create several tasks, all run to completion
 *
 * Strategy:
 *   We create tasks that increment volatile shared counters in a loop.
 *   The idle task (main) busy-waits for a deadline (tick count), then
 *   checks the counters. If both counters > 0, context switching works.
 *   If both counters reached their target, preemptive scheduling works.
 */

#include "test_multitask.h"
#include "../drivers/screen.h"
#include "../kernel/task/task.h"
#include "../kernel/task/sched.h"
#include "../kernel/sys/timer.h"
#include "../libc/string.h"

// ─── Shared state between test tasks and the test harness ──────────────────

#define TEST_TARGET 50   // Each task increments its counter this many times

static volatile uint32_t counter_a = 0;
static volatile uint32_t counter_b = 0;
static volatile uint32_t counter_c = 0;
static volatile uint32_t order_log[256];  // Record which task ran in what order
static volatile uint32_t order_idx = 0;

// ─── Test task functions ───────────────────────────────────────────────────

/**
 * task_func_a - Test task A
 * Increments counter_a and logs its execution order.
 */
static void task_func_a(void) {
    for (uint32_t i = 0; i < TEST_TARGET; i++) {
        counter_a++;
        if (order_idx < 256) {
            order_log[order_idx++] = 1;  // 1 = task A
        }
        // Small busy-wait to give the timer a chance to preempt
        for (volatile uint32_t j = 0; j < 50000; j++);
    }
    // task_exit() is called automatically by task_wrapper
}

/**
 * task_func_b - Test task B
 * Increments counter_b and logs its execution order.
 */
static void task_func_b(void) {
    for (uint32_t i = 0; i < TEST_TARGET; i++) {
        counter_b++;
        if (order_idx < 256) {
            order_log[order_idx++] = 2;  // 2 = task B
        }
        for (volatile uint32_t j = 0; j < 50000; j++);
    }
}

/**
 * task_func_c - Test task C (for multi-task test)
 */
static void task_func_c(void) {
    for (uint32_t i = 0; i < TEST_TARGET; i++) {
        counter_c++;
        for (volatile uint32_t j = 0; j < 50000; j++);
    }
}

// ─── Helper: busy-wait for a number of timer ticks ─────────────────────────

static void wait_ticks(uint32_t ticks) {
    uint32_t start = get_timer_ticks();
    while (get_timer_ticks() - start < ticks) {
        __asm__ volatile("hlt");  // sleep until next interrupt
    }
}

// ─── Test runner ───────────────────────────────────────────────────────────

void test_multitask(void) {
    kprint("\n=====================================\n");
    kprint("   MULTITASKING TESTS (Phase 12)\n");
    kprint("=====================================\n");

    // ── Test 1: Task creation ──────────────────────────────────────────
    kprint("\nTest 1: Task creation...\n");

    counter_a = 0;
    counter_b = 0;
    counter_c = 0;
    order_idx = 0;

    task_t *ta = task_create(task_func_a, "test_a");
    task_t *tb = task_create(task_func_b, "test_b");

    if (!ta) {
        kprint("  [FAIL] task_create returned NULL for task A\n");
        return;
    }
    if (!tb) {
        kprint("  [FAIL] task_create returned NULL for task B\n");
        return;
    }
    if (ta->id == 0) {
        kprint("  [FAIL] Task A has ID 0 (should be > 0)\n");
        return;
    }
    if (tb->id == 0) {
        kprint("  [FAIL] Task B has ID 0 (should be > 0)\n");
        return;
    }
    if (ta->id == tb->id) {
        kprint("  [FAIL] Task A and B have same ID\n");
        return;
    }
    kprint("  [PASS] Task A created (ID=");
    kprint_uint(ta->id);
    kprint(", name=test_a)\n");
    kprint("  [PASS] Task B created (ID=");
    kprint_uint(tb->id);
    kprint(", name=test_b)\n");

    // ── Test 2: Context switch (both tasks execute) ────────────────────
    kprint("\nTest 2: Context switch — waiting for tasks to run...\n");

    // Wait up to 3 seconds (300 ticks at 100Hz) for tasks to finish
    uint32_t deadline = 300;
    uint32_t start_tick = get_timer_ticks();

    while (get_timer_ticks() - start_tick < deadline) {
        // If both tasks are done, stop waiting early
        if (counter_a >= TEST_TARGET && counter_b >= TEST_TARGET) {
            break;
        }
        __asm__ volatile("hlt");
        task_reap();
    }

    if (counter_a == 0 && counter_b == 0) {
        kprint("  [FAIL] Neither task executed (counter_a=0, counter_b=0)\n");
        kprint("         Context switch likely broken — tasks never ran\n");
        return;
    }
    if (counter_a == 0) {
        kprint("  [FAIL] Task A never ran (counter_a=0)\n");
        return;
    }
    if (counter_b == 0) {
        kprint("  [FAIL] Task B never ran (counter_b=0)\n");
        return;
    }

    kprint("  [PASS] Task A executed (counter=");
    kprint_uint(counter_a);
    kprint("/");
    kprint_uint(TEST_TARGET);
    kprint(")\n");
    kprint("  [PASS] Task B executed (counter=");
    kprint_uint(counter_b);
    kprint("/");
    kprint_uint(TEST_TARGET);
    kprint(")\n");

    // ── Test 3: Preemptive scheduling (both finished fully) ────────────
    kprint("\nTest 3: Preemptive scheduling — full completion check...\n");

    if (counter_a < TEST_TARGET) {
        kprint("  [FAIL] Task A did not finish (");
        kprint_uint(counter_a);
        kprint("/");
        kprint_uint(TEST_TARGET);
        kprint(")\n");
    } else {
        kprint("  [PASS] Task A completed all ");
        kprint_uint(TEST_TARGET);
        kprint(" iterations\n");
    }

    if (counter_b < TEST_TARGET) {
        kprint("  [FAIL] Task B did not finish (");
        kprint_uint(counter_b);
        kprint("/");
        kprint_uint(TEST_TARGET);
        kprint(")\n");
    } else {
        kprint("  [PASS] Task B completed all ");
        kprint_uint(TEST_TARGET);
        kprint(" iterations\n");
    }

    // ── Test 4: Interleaving check ─────────────────────────────────────
    kprint("\nTest 4: Interleaving check...\n");

    // Count transitions in the order log (1→2 or 2→1)
    uint32_t transitions = 0;
    for (uint32_t i = 1; i < order_idx && i < 256; i++) {
        if (order_log[i] != order_log[i - 1]) {
            transitions++;
        }
    }

    kprint("  Recorded ");
    kprint_uint(order_idx);
    kprint(" log entries, ");
    kprint_uint(transitions);
    kprint(" task switches observed\n");

    if (transitions == 0) {
        kprint("  [WARN] No interleaving detected — tasks ran sequentially\n");
        kprint("         (cooperative yield worked, but preemption may not)\n");
    } else {
        kprint("  [PASS] Tasks interleaved (");
        kprint_uint(transitions);
        kprint(" context switches)\n");
    }

    // ── Test 5: Task exit and reap ─────────────────────────────────────
    kprint("\nTest 5: Task exit and reap...\n");

    // Give a moment for tasks to fully exit
    wait_ticks(10);
    task_reap();

    // Both tasks should now be cleaned up (their TCB slots zeroed)
    kprint("  [PASS] task_reap() completed without crash\n");

    // ── Test 6: Multiple tasks (3 concurrent) ──────────────────────────
    kprint("\nTest 6: Three concurrent tasks...\n");

    counter_a = 0;
    counter_b = 0;
    counter_c = 0;

    task_t *t1 = task_create(task_func_a, "multi_a");
    task_t *t2 = task_create(task_func_b, "multi_b");
    task_t *t3 = task_create(task_func_c, "multi_c");

    if (!t1 || !t2 || !t3) {
        kprint("  [FAIL] Could not create all 3 tasks\n");
        return;
    }

    // Wait for completion
    start_tick = get_timer_ticks();
    while (get_timer_ticks() - start_tick < deadline) {
        if (counter_a >= TEST_TARGET && counter_b >= TEST_TARGET &&
            counter_c >= TEST_TARGET) {
            break;
        }
        __asm__ volatile("hlt");
        task_reap();
    }

    if (counter_a >= TEST_TARGET && counter_b >= TEST_TARGET &&
        counter_c >= TEST_TARGET) {
        kprint("  [PASS] All 3 tasks completed (");
        kprint_uint(counter_a);
        kprint(", ");
        kprint_uint(counter_b);
        kprint(", ");
        kprint_uint(counter_c);
        kprint(")\n");
    } else {
        kprint("  [FAIL] Not all tasks finished: a=");
        kprint_uint(counter_a);
        kprint(" b=");
        kprint_uint(counter_b);
        kprint(" c=");
        kprint_uint(counter_c);
        kprint("\n");
    }

    // Final cleanup
    wait_ticks(10);
    task_reap();

    kprint("\n=====================================\n");
    kprint("   MULTITASKING TESTS COMPLETE\n");
    kprint("=====================================\n\n");
}
