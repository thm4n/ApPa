/**
 * task.h - Task Control Block and Task Management
 *
 * Defines the per-task state structure and the API for creating,
 * destroying, and managing kernel tasks.
 *
 * Each task owns:
 *   - A unique ID
 *   - A 4 KB kernel stack (allocated from the PMM)
 *   - A saved ESP that the context switch restores
 *   - A link pointer for the ready queue
 */

#ifndef TASK_H
#define TASK_H

#include "../../libc/stdint.h"

// ─── Constants ─────────────────────────────────────────────────────────────

#define TASK_KERNEL_STACK_SIZE  4096   // 4 KB per kernel stack
#define TASK_NAME_MAX           32     // Max task name length (incl. NUL)
#define TASK_MAX_TASKS          64     // Hard cap on concurrent tasks

// ─── Task States ───────────────────────────────────────────────────────────

typedef enum {
    TASK_READY,     // In the ready queue, eligible to run
    TASK_RUNNING,   // Currently executing on the CPU
    TASK_BLOCKED,   // Waiting for an event (future: sleep, I/O, mutex)
    TASK_DEAD       // Finished; awaiting cleanup
} task_state_t;

// ─── Task Control Block ────────────────────────────────────────────────────

typedef struct task {
    uint32_t        id;                     // Unique task identifier
    char            name[TASK_NAME_MAX];    // Human-readable name
    task_state_t    state;                  // Current scheduling state

    uint32_t        esp;                    // Saved stack pointer (context switch)
    uint32_t        esp0;                   // Kernel stack top (written to TSS)
    uint32_t        *kernel_stack;          // Base of allocated kernel stack page

    uint32_t        *user_stack;            // Base of allocated user stack page (NULL for kernel tasks)
    uint32_t        user_stack_top;         // Top of user stack (used in iret frame)
    uint8_t         is_user;                // 1 = Ring 3 task, 0 = Ring 0 task

    uint32_t        cr3;                    // Physical address of per-process page directory (0 = kernel)
    void            *page_dir;              // Virtual pointer to page directory (NULL for kernel tasks)

    struct task     *next;                  // Next task in the ready / free list
} task_t;

// ─── Task Entry Point ──────────────────────────────────────────────────────

/** Function signature for a task's main body. */
typedef void (*task_entry_t)(void);

// ─── API ───────────────────────────────────────────────────────────────────

/**
 * task_create - Spawn a new kernel task
 * @entry: Function the task will execute
 * @name:  Human-readable name (up to TASK_NAME_MAX-1 chars)
 *
 * Allocates a kernel stack, pre-populates a context frame so the
 * first context switch "returns" into @entry, and adds the task
 * to the scheduler's ready queue.
 *
 * Returns: Pointer to the new task, or NULL on failure.
 */
task_t* task_create(task_entry_t entry, const char *name);

/**
 * task_exit - Terminate the currently running task
 *
 * Marks the current task as TASK_DEAD and yields to the scheduler.
 * The idle loop or a reaper will eventually free the task's resources.
 */
void task_exit(void);

/**
 * task_get_current - Return the currently running task
 */
task_t* task_get_current(void);

/**
 * task_reap - Free resources of all DEAD tasks
 *
 * Called periodically (e.g. from the idle task) to reclaim
 * kernel stacks and TCB slots.
 */
void task_reap(void);

/**
 * task_create_user - Spawn a new Ring 3 (user-mode) task
 * @entry: Entry point that will execute in user mode
 * @name:  Human-readable name
 *
 * Allocates both a kernel stack and a user stack, builds a fake
 * iret frame on the kernel stack so the first context switch drops
 * into Ring 3 at @entry via enter_usermode.
 *
 * Returns: Pointer to the new task, or NULL on failure.
 */
task_t* task_create_user(task_entry_t entry, const char *name);

#endif // TASK_H
