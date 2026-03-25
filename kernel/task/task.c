/**
 * task.c - Task creation, destruction, and resource management
 *
 * Provides task_create() which allocates a kernel stack from the PMM,
 * pre-populates a context frame so the scheduler can "return" into the
 * task's entry point, and wires the task into the ready queue.
 */

#include "task.h"
#include "sched.h"
#include "../mem/pmm.h"
#include "../arch/gdt.h"
#include "../../libc/string.h"

// ─── Static task pool (avoids kmalloc dependency for TCBs) ─────────────────

static task_t task_pool[TASK_MAX_TASKS];
static uint32_t next_task_id = 0;

// ─── Internal helpers ──────────────────────────────────────────────────────

/**
 * alloc_tcb - Grab the next free slot from the static pool
 */
static task_t* alloc_tcb(void) {
    for (uint32_t i = 0; i < TASK_MAX_TASKS; i++) {
        if (task_pool[i].state == TASK_DEAD && task_pool[i].id == 0 &&
            task_pool[i].kernel_stack == 0) {
            return &task_pool[i];
        }
    }
    return 0;  // Pool exhausted
}

/**
 * task_wrapper - Wrapper that calls the real entry point and then task_exit()
 *
 * If a task function returns instead of calling task_exit(), this
 * wrapper catches it and does the cleanup.
 */
static void task_wrapper(void) {
    // The entry point address was placed just above the context frame
    // by task_create(). We recover it via inline asm — it's in EBX,
    // which was set to the entry pointer when we built the fake frame.
    task_entry_t entry;
    __asm__ volatile("mov %%ebx, %0" : "=r"(entry));

    // Run the actual task body
    entry();

    // If the task returns, clean up
    task_exit();
}

// ─── Public API ────────────────────────────────────────────────────────────

/**
 * task_create - Spawn a new kernel task
 */
task_t* task_create(task_entry_t entry, const char *name) {
    task_t *t = alloc_tcb();
    if (!t) return 0;

    // Assign identity
    t->id = ++next_task_id;
    strncpy(t->name, name, TASK_NAME_MAX - 1);
    t->name[TASK_NAME_MAX - 1] = '\0';

    // Allocate a 4 KB kernel stack page from PMM
    uint32_t stack_phys = alloc_page();
    if (!stack_phys) {
        memset(t, 0, sizeof(task_t));
        return 0;
    }
    t->kernel_stack = (uint32_t*)stack_phys;
    t->esp0 = stack_phys + TASK_KERNEL_STACK_SIZE;  // Top of stack

    // ── Build a fake context frame on the new stack ────────────────────
    //
    // The context switch (task_switch in switch.asm) does:
    //   pop ebp; pop edi; pop esi; pop ebx; ret
    //
    // So we push:  entry_trampoline (return address)
    //              ebx = real entry pointer
    //              esi = 0, edi = 0, ebp = 0
    //
    // When the scheduler switches to this task for the first time,
    // it will pop ebx = entry, then "ret" into task_wrapper.

    uint32_t *sp = (uint32_t*)(t->esp0);

    // --- fake frame (pushed in reverse since stack grows down) ---
    *(--sp) = (uint32_t)task_wrapper;   // return address for 'ret'
    *(--sp) = 0;                        // ebp
    *(--sp) = 0;                        // edi
    *(--sp) = 0;                        // esi
    *(--sp) = (uint32_t)entry;          // ebx = real entry point

    t->esp = (uint32_t)sp;
    t->state = TASK_READY;
    t->next = 0;

    // Add to the scheduler's ready queue
    sched_add_task(t);

    return t;
}

/**
 * task_exit - Terminate the current task
 */
void task_exit(void) {
    task_t *cur = task_get_current();
    if (cur) {
        cur->state = TASK_DEAD;
    }
    // Yield to the scheduler — we'll never come back
    sched_yield();

    // Safety: should never reach here
    for (;;) { __asm__ volatile("hlt"); }
}

/**
 * task_get_current - Return the currently running task
 */
task_t* task_get_current(void) {
    return sched_get_current();
}

/**
 * task_reap - Free resources of all DEAD tasks
 *
 * Scans the pool and frees kernel stacks of dead tasks so pages
 * can be reused.
 */
void task_reap(void) {
    for (uint32_t i = 0; i < TASK_MAX_TASKS; i++) {
        if (task_pool[i].state == TASK_DEAD && task_pool[i].kernel_stack != 0) {
            free_page((uint32_t)task_pool[i].kernel_stack);
            memset(&task_pool[i], 0, sizeof(task_t));
        }
    }
}
