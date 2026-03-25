/**
 * test_multitask.h - Multitasking subsystem tests
 *
 * Unlike other unit tests, these run AFTER sched_enable() because
 * they need the scheduler and timer interrupt to be active.
 * The test spawns tasks and verifies context switching, task lifecycle,
 * and preemptive scheduling by observing shared counters.
 */

#ifndef TEST_MULTITASK_H
#define TEST_MULTITASK_H

/**
 * test_multitask - Run all multitasking tests
 *
 * Must be called after sched_init() + sti + sched_enable().
 * Spawns test tasks, waits for them to complete, then reports results.
 */
void test_multitask(void);

#endif // TEST_MULTITASK_H
