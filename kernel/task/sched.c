/**
 * sched.c - Round-Robin Preemptive Scheduler
 *
 * Maintains a circular linked-list ready queue. The timer ISR
 * calls schedule() which decrements a time-slice counter and
 * performs a context switch when it expires.
 *
 * The first task ("kernel") is synthesised from the current
 * execution context by sched_init(); it becomes the idle task
 * once other tasks are created.
 */

#include "sched.h"
#include "task.h"
#include "../arch/tss.h"
#include "../arch/gdt.h"
#include "../mem/paging.h"
#include "../../klibc/string.h"
#include "../../drivers/screen.h"

// ─── External: low-level context switch (switch.asm) ───────────────────────

extern void task_switch(uint32_t *old_esp, uint32_t new_esp);

// ─── Module state ──────────────────────────────────────────────────────────

static task_t  boot_task;               // TCB for the bootstrap/idle task
static task_t *current_task  = 0;       // Currently executing task
static task_t *ready_head    = 0;       // Head of the circular ready queue
static int     scheduler_on  = 0;       // 1 = preemptive scheduling active
static uint32_t slice_remaining = 0;    // Ticks left in current time-slice

// ─── Internal helpers ──────────────────────────────────────────────────────

/**
 * insert_after - Splice @task into the circular list after @after
 */
static void insert_after(task_t *after, task_t *task) {
    task->next  = after->next;
    after->next = task;
}

/**
 * pick_next_ready - Walk the ready queue starting from current_task
 *                   and return the next READY (or RUNNING) task.
 *                   Skips DEAD and BLOCKED tasks.
 */
static task_t* pick_next_ready(void) {
    task_t *t = current_task->next;
    task_t *start = t;

    do {
        if (t->state == TASK_READY || t->state == TASK_RUNNING) {
            return t;
        }
        t = t->next;
    } while (t != start);

    // No other ready task — keep running current
    return current_task;
}

/**
 * switch_to - Perform the actual context switch from current to @next
 */
static void switch_to(task_t *next) {
    if (next == current_task) return;   // Nothing to do

    task_t *prev = current_task;

    // Only demote to READY if the task was actually RUNNING.
    // A DEAD task (from task_exit) must stay DEAD so the reaper
    // can find and free it.
    if (prev->state == TASK_RUNNING) {
        prev->state = TASK_READY;
    }
    next->state = TASK_RUNNING;
    current_task = next;

    // Reset time-slice for the new task
    slice_remaining = SCHED_TIME_SLICE;

    // Update TSS so interrupts in the new task use its kernel stack
    tss_set_kernel_stack(next->esp0);

    // ── Phase 15: Switch address space if needed ──
    // User tasks have a per-process page directory (cr3 != 0).
    // Kernel/idle tasks use cr3 == 0, meaning "kernel directory".
    {
        uint32_t next_cr3 = next->cr3 ? next->cr3 : paging_get_kernel_cr3();
        uint32_t prev_cr3 = prev->cr3 ? prev->cr3 : paging_get_kernel_cr3();
        if (next_cr3 != prev_cr3) {
            __asm__ volatile("mov %0, %%cr3" : : "r"(next_cr3) : "memory");
        }
    }

    // Low-level register swap
    task_switch(&prev->esp, next->esp);
}

// ─── Public API ────────────────────────────────────────────────────────────

void sched_init(void) {
    // Build a TCB that represents the currently executing context
    memset(&boot_task, 0, sizeof(task_t));
    boot_task.id    = 0;
    strncpy(boot_task.name, "idle", TASK_NAME_MAX);
    boot_task.state = TASK_RUNNING;
    // esp will be filled in on first context switch (task_switch writes it)
    // esp0 is the current kernel stack top (we're already using it)
    // kernel_stack is NULL — this task's stack is the original boot stack,
    //   not PMM-allocated, so we must never free it.
    boot_task.next  = &boot_task;   // Circular list of one

    current_task    = &boot_task;
    ready_head      = &boot_task;
    slice_remaining = SCHED_TIME_SLICE;
    scheduler_on    = 0;
}

void sched_enable(void) {
    scheduler_on = 1;
}

void sched_disable(void) {
    scheduler_on = 0;
}

void sched_add_task(task_t *task) {
    if (!task) return;

    // Disable interrupts while touching the queue
    __asm__ volatile("cli");

    if (!ready_head) {
        // Empty queue — make it the sole entry
        task->next  = task;
        ready_head  = task;
    } else {
        // Insert after the current tail (just before ready_head wraps)
        // Walk to the node whose ->next == ready_head (tail of circle)
        task_t *tail = ready_head;
        while (tail->next != ready_head) {
            tail = tail->next;
        }
        insert_after(tail, task);
    }

    __asm__ volatile("sti");
}

void sched_yield(void) {
    __asm__ volatile("cli");

    task_t *next = pick_next_ready();
    switch_to(next);

    __asm__ volatile("sti");
}

void schedule(void) {
    if (!scheduler_on) return;

    // Decrement time-slice
    if (slice_remaining > 0) {
        slice_remaining--;
        if (slice_remaining > 0) return; // Still has time left
    }

    // Time-slice expired — switch to next ready task
    task_t *next = pick_next_ready();
    switch_to(next);
}

task_t* sched_get_current(void) {
    return current_task;
}

int sched_is_enabled(void) {
    return scheduler_on;
}

void sched_remove_task(task_t *task) {
    if (!task || !ready_head) return;

    // Never remove the boot/idle task
    if (task == &boot_task) return;

    __asm__ volatile("cli");

    // Single-node circle (shouldn't happen — boot_task is always present)
    if (task->next == task) {
        ready_head = 0;
        task->next = 0;
        __asm__ volatile("sti");
        return;
    }

    // Walk the circle to find the predecessor
    task_t *prev = ready_head;
    task_t *start = prev;
    do {
        if (prev->next == task) {
            prev->next = task->next;
            if (ready_head == task) {
                ready_head = task->next;
            }
            task->next = 0;
            __asm__ volatile("sti");
            return;
        }
        prev = prev->next;
    } while (prev != start);

    // Not found — already removed or never added
    __asm__ volatile("sti");
}
