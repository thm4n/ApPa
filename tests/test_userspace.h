/**
 * test_userspace.h - Ring 3 userspace tests
 *
 * Tests user-mode task creation, syscall dispatch (write, getpid, yield,
 * exit), and GPF handling when a user task does something illegal.
 *
 * Must be called after syscall_init() + sched_enable().
 */

#ifndef TEST_USERSPACE_H
#define TEST_USERSPACE_H

/**
 * test_userspace - Run all Ring 3 / syscall tests
 *
 * Spawns user-mode tasks via task_create_user(), waits for them
 * to finish, and checks that syscalls worked and GPF was caught.
 */
void test_userspace(void);

#endif /* TEST_USERSPACE_H */
