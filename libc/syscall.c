/**
 * syscall.c — User-space system call implementation
 *
 * All syscalls go through INT 0x80 with the standard ApPa ABI:
 *   EAX = syscall number, EBX = arg1, ECX = arg2
 *   Return value in EAX.
 *
 * No kernel headers are included — this is pure freestanding C.
 */

#include "syscall.h"

/* ── Raw INT 0x80 helpers ─────────────────────────────────────────────────── */

static inline int _syscall0(int num) {
    int ret;
    __asm__ volatile("int $0x80"
        : "=a"(ret)
        : "a"(num)
        : "memory");
    return ret;
}

static inline int _syscall1(int num, int a1) {
    int ret;
    __asm__ volatile("int $0x80"
        : "=a"(ret)
        : "a"(num), "b"(a1)
        : "memory");
    return ret;
}

static inline int _syscall2(int num, int a1, int a2) {
    int ret;
    __asm__ volatile("int $0x80"
        : "=a"(ret)
        : "a"(num), "b"(a1), "c"(a2)
        : "memory");
    return ret;
}

/* ── Syscall wrappers ─────────────────────────────────────────────────────── */

void sys_exit(int code) {
    _syscall1(SYS_EXIT, code);
    /* Should not return, but just in case: */
    for (;;) __asm__ volatile("hlt");
}

int sys_write(const char *buf, int len) {
    return _syscall2(SYS_WRITE, (int)buf, len);
}

int sys_read(char *buf, int max) {
    return _syscall2(SYS_READ, (int)buf, max);
}

int sys_getpid(void) {
    return _syscall0(SYS_GETPID);
}

void sys_yield(void) {
    _syscall0(SYS_YIELD);
}

void sys_sleep(int ms) {
    _syscall1(SYS_SLEEP, ms);
}
