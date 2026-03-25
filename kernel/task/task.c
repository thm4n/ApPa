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
#include "../mem/paging.h"
#include "../arch/gdt.h"
#include "../../libc/string.h"

// ─── Static task pool (avoids kmalloc dependency for TCBs) ─────────────────

static task_t task_pool[TASK_MAX_TASKS];
static uint32_t next_task_id = 0;

// ─── Internal helpers ──────────────────────────────────────────────────────

/**
 * alloc_tcb - Grab the next free slot from the static pool
 *
 * A slot is free when id == 0 and kernel_stack == 0.
 * This holds for both never-used (static-zeroed) and reaped slots.
 */
static task_t* alloc_tcb(void) {
    for (uint32_t i = 0; i < TASK_MAX_TASKS; i++) {
        if (task_pool[i].id == 0 && task_pool[i].kernel_stack == 0) {
            return &task_pool[i];
        }
    }
    return 0;  // Pool exhausted
}

/**
 * task_wrapper - Trampoline for new tasks
 *
 * Called via a fake stack frame built by task_create().  The entry
 * address is passed as a cdecl parameter (on the stack), avoiding
 * any reliance on a specific register value at function entry.
 *
 * Enables interrupts (sti) because the context switch that brought
 * us here happened inside a timer ISR with IF=0.
 */
static void task_wrapper(uint32_t entry_addr) {
    // Re-enable interrupts — we arrived here via task_switch which
    // executed inside a timer ISR (IF=0).  Without sti the task
    // would never be preempted and no other interrupts would fire.
    __asm__ volatile("sti");

    // Cast and call the real entry point
    task_entry_t entry = (task_entry_t)entry_addr;
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
    // task_switch (switch.asm) restores: pop ebp, pop edi, pop esi,
    // pop ebx, then ret.  We lay the stack out so ret lands in
    // task_wrapper, which receives `entry` as a cdecl argument.
    //
    // Stack layout (addresses increase upward):
    //
    //   [high]  entry          ← arg1 for task_wrapper  (cdecl [ebp+8])
    //           0x00000000     ← fake return address     (cdecl [ebp+4])
    //           task_wrapper   ← popped by 'ret'
    //           0  (ebx)       ← popped 4th
    //           0  (esi)       ← popped 3rd
    //           0  (edi)       ← popped 2nd
    //   [low]   0  (ebp)       ← popped 1st  ← t->esp

    uint32_t *sp = (uint32_t*)(t->esp0);

    *(--sp) = (uint32_t)entry;          // cdecl arg1 for task_wrapper
    *(--sp) = 0;                        // fake return address
    *(--sp) = (uint32_t)task_wrapper;   // return address for 'ret'
    *(--sp) = 0;                        // ebx
    *(--sp) = 0;                        // esi
    *(--sp) = 0;                        // edi
    *(--sp) = 0;                        // ebp

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
 * Scans the pool, unlinks dead tasks from the scheduler's ready
 * queue, frees their kernel stacks, and zeroes the TCB so the
 * slot can be reused by alloc_tcb().
 */
void task_reap(void) {
    __asm__ volatile("cli");

    for (uint32_t i = 0; i < TASK_MAX_TASKS; i++) {
        if (task_pool[i].state == TASK_DEAD && task_pool[i].kernel_stack != 0) {
            // Unlink from the scheduler's circular ready queue first
            sched_remove_task(&task_pool[i]);

            // Free the PMM page used as the kernel stack
            free_page((uint32_t)task_pool[i].kernel_stack);

            // Free user stack if this was a Ring 3 task
            if (task_pool[i].user_stack != 0) {
                // Unmap the user-accessible page
                paging_unmap_page((uint32_t)task_pool[i].user_stack);
                free_page((uint32_t)task_pool[i].user_stack);
            }

            // Zero the slot so alloc_tcb recognises it as free
            // (id == 0 && kernel_stack == 0)
            memset(&task_pool[i], 0, sizeof(task_t));
        }
    }

    __asm__ volatile("sti");
}

/* ─── User-mode trampoline (defined in umode.asm) ──────────────────────── */

extern void enter_usermode(void);

/**
 * task_create_user - Spawn a new Ring 3 (user-mode) task
 */
task_t* task_create_user(task_entry_t entry, const char *name) {
    task_t *t = alloc_tcb();
    if (!t) return 0;

    /* Assign identity */
    t->id = ++next_task_id;
    strncpy(t->name, name, TASK_NAME_MAX - 1);
    t->name[TASK_NAME_MAX - 1] = '\0';
    t->is_user = 1;

    /* ── Allocate kernel stack (4 KB, supervisor only) ─────────────── */
    uint32_t kstack_phys = alloc_page();
    if (!kstack_phys) {
        memset(t, 0, sizeof(task_t));
        return 0;
    }
    t->kernel_stack = (uint32_t*)kstack_phys;
    t->esp0 = kstack_phys + TASK_KERNEL_STACK_SIZE;   /* Top of kernel stack */

    /* ── Allocate user stack (4 KB, user-accessible) ───────────────── */
    uint32_t ustack_phys = alloc_page();
    if (!ustack_phys) {
        free_page(kstack_phys);
        memset(t, 0, sizeof(task_t));
        return 0;
    }
    t->user_stack = (uint32_t*)ustack_phys;

    /* Map the user stack page with User/Supervisor=1 so Ring 3 can use it.
     * Under identity mapping physical == virtual. */
    paging_map_user(ustack_phys, ustack_phys, 1 /* writable */);
    t->user_stack_top = ustack_phys + TASK_KERNEL_STACK_SIZE;  /* Same 4 KB */

    /* ── Build iret frame on the kernel stack ──────────────────────── *
     *
     * The first context switch (task_switch) will pop ebp/edi/esi/ebx
     * then 'ret' into enter_usermode.  enter_usermode then does 'iret'
     * which pops: EIP, CS, EFLAGS, ESP, SS — dropping us into Ring 3.
     *
     * Kernel stack layout (addresses increase upward):
     *
     *   [high]  USER_DATA_SEG  (0x23)   ← SS  for iret
     *           user_stack_top           ← ESP for iret
     *           EFLAGS | 0x200          ← IF=1 (interrupts enabled)
     *           USER_CODE_SEG  (0x1B)   ← CS  for iret
     *           entry                   ← EIP for iret
     *           enter_usermode          ← ret address for task_switch
     *           0  (ebx)
     *           0  (esi)
     *           0  (edi)
     *   [low]   0  (ebp)               ← t->esp
     */
    uint32_t *sp = (uint32_t*)(t->esp0);

    /* iret frame (popped by the CPU on iret) */
    *(--sp) = GDT_USER_DATA_SEG;              /* SS   */
    *(--sp) = t->user_stack_top;              /* ESP  */
    *(--sp) = 0x202;                          /* EFLAGS: IF=1, reserved bit 1 */
    *(--sp) = GDT_USER_CODE_SEG;              /* CS   */
    *(--sp) = (uint32_t)entry;                /* EIP  */

    /* task_switch frame (popped by switch.asm) */
    *(--sp) = (uint32_t)enter_usermode;       /* ret → enter_usermode */
    *(--sp) = 0;                              /* ebx */
    *(--sp) = 0;                              /* esi */
    *(--sp) = 0;                              /* edi */
    *(--sp) = 0;                              /* ebp */

    t->esp   = (uint32_t)sp;
    t->state = TASK_READY;
    t->next  = 0;

    /* Add to the scheduler's ready queue */
    sched_add_task(t);

    return t;
}
