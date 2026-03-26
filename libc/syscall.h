/**
 * syscall.h — User-space system call interface
 *
 * Provides wrappers around the INT 0x80 syscall ABI used by ApPa.
 * Each wrapper maps to a kernel handler in kernel/arch/syscall.c.
 *
 * ABI:
 *   EAX = syscall number
 *   EBX = arg1, ECX = arg2
 *   Return value in EAX
 */

#ifndef LIBC_SYSCALL_H
#define LIBC_SYSCALL_H

/* ── Syscall numbers (must match kernel/arch/syscall.h) ───────────────────── */

#define SYS_EXIT    0
#define SYS_WRITE   1
#define SYS_READ    2
#define SYS_YIELD   3
#define SYS_GETPID  4
#define SYS_SLEEP   5

/* ── Syscall wrappers ─────────────────────────────────────────────────────── */

/** Terminate the current task with exit code.  Does not return. */
void  sys_exit(int code);

/** Write @len bytes from @buf to the screen.  Returns bytes written or -1. */
int   sys_write(const char *buf, int len);

/** Read up to @max bytes from keyboard into @buf.  Returns bytes read or -1. */
int   sys_read(char *buf, int max);

/** Get current task ID. */
int   sys_getpid(void);

/** Voluntarily yield the CPU to the next ready task. */
void  sys_yield(void);

/** Sleep for @ms milliseconds. */
void  sys_sleep(int ms);

#endif /* LIBC_SYSCALL_H */
