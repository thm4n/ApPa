/**
 * sched.h - Round-Robin Preemptive Scheduler
 *
 * Manages the ready queue and drives context switches via the
 * timer interrupt (IRQ0).  The scheduler can be enabled/disabled
 * at runtime so that boot-time init code runs without preemption.
 */

#ifndef SCHED_H
#define SCHED_H

#include "task.h"

// ─── Scheduler time-slice ──────────────────────────────────────────────────

/** Number of timer ticks per time slice (10 ticks = 100 ms at 100 Hz). */
#define SCHED_TIME_SLICE  10

// ─── API ───────────────────────────────────────────────────────────────────

/**
 * sched_init - Initialise the scheduler and create the "kernel" bootstrap task
 *
 * Wraps the currently executing context (main's stack) into a task_t
 * so that subsequent task_create() + sched_enable() can schedule it
 * alongside other tasks.
 *
 * Must be called AFTER all subsystem init is done but BEFORE task_create().
 */
void sched_init(void);

/**
 * sched_enable - Start preemptive scheduling
 *
 * After this call, the timer handler will invoke schedule() on every
 * time-slice expiry.
 */
void sched_enable(void);

/**
 * sched_disable - Stop preemptive scheduling
 *
 * Useful when running critical kernel init or shutdown code that
 * must not be preempted.
 */
void sched_disable(void);

/**
 * sched_add_task - Insert a READY task into the ready queue
 * @task: Task to add (must already have state == TASK_READY)
 */
void sched_add_task(task_t *task);

/**
 * sched_yield - Voluntarily give up the CPU
 *
 * Performs an immediate round-robin switch to the next ready task.
 * Useful for cooperative points in long-running kernel work.
 */
void sched_yield(void);

/**
 * schedule - Core scheduling decision (called from timer IRQ)
 *
 * Decrements the current task's remaining time-slice. When the slice
 * expires, picks the next READY task and performs a context switch.
 */
void schedule(void);

/**
 * sched_get_current - Return the currently running task
 */
task_t* sched_get_current(void);

/**
 * sched_is_enabled - Check if preemptive scheduling is active
 * Returns: 1 if enabled, 0 if disabled
 */
int sched_is_enabled(void);

/**
 * sched_remove_task - Unlink a task from the circular ready queue
 * @task: Task to remove (must not be the boot/idle task)
 *
 * Called by task_reap() before freeing a dead task's resources so
 * the circular linked list is not corrupted.
 */
void sched_remove_task(task_t *task);

#endif // SCHED_H
