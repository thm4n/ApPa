/**
 * syscall.h - User-space system call wrappers
 *
 * Provides inline INT 0x80 invocation and convenience functions that
 * user-mode programs call instead of touching kernel internals.
 *
 * Calling convention (matches Linux i386):
 *   EAX = syscall number
 *   EBX = arg1, ECX = arg2, EDX = arg3, ESI = arg4, EDI = arg5
 *   EAX = return value
 */

#ifndef LIBC_SYSCALL_H
#define LIBC_SYSCALL_H

#include "stdint.h"

/* ─── Syscall numbers (must mirror kernel/arch/syscall.h) ───────────────── */

#define SYS_EXIT    0
#define SYS_WRITE   1
#define SYS_READ    2
#define SYS_YIELD   3
#define SYS_GETPID  4
#define SYS_SLEEP   5

/* ─── Raw INT 0x80 helpers ──────────────────────────────────────────────── */

static inline int syscall0(int num) {
    int ret;
    __asm__ volatile(
        "int $0x80"
        : "=a"(ret)
        : "a"(num)
        : "memory"
    );
    return ret;
}

static inline int syscall1(int num, int a1) {
    int ret;
    __asm__ volatile(
        "int $0x80"
        : "=a"(ret)
        : "a"(num), "b"(a1)
        : "memory"
    );
    return ret;
}

static inline int syscall2(int num, int a1, int a2) {
    int ret;
    __asm__ volatile(
        "int $0x80"
        : "=a"(ret)
        : "a"(num), "b"(a1), "c"(a2)
        : "memory"
    );
    return ret;
}

static inline int syscall3(int num, int a1, int a2, int a3) {
    int ret;
    __asm__ volatile(
        "int $0x80"
        : "=a"(ret)
        : "a"(num), "b"(a1), "c"(a2), "d"(a3)
        : "memory"
    );
    return ret;
}

/* ─── Convenience wrappers ──────────────────────────────────────────────── */

/** Terminate the current task. Does not return. */
void sys_exit(void);

/** Write len bytes from buf to the screen. Returns bytes written or -1. */
int sys_write(const char *buf, int len);

/** Read up to max bytes from keyboard into buf. Returns bytes read or -1. */
int sys_read(char *buf, int max);

/** Voluntarily yield the CPU to the next ready task. */
void sys_yield(void);

/** Get the current task's ID. */
int sys_getpid(void);

/** Sleep for ms milliseconds. */
void sys_sleep(int ms);

#endif /* LIBC_SYSCALL_H */
