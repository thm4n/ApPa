/**
 * ulibc.c — User-space libc implementation for ApPa
 *
 * All syscalls go through INT 0x80 with the standard ApPa ABI:
 *   EAX = syscall number, EBX = arg1, ECX = arg2
 *   Return value in EAX.
 *
 * No kernel headers are included — this is pure freestanding C.
 */

#include "ulibc.h"

/* ── Syscall numbers (must match kernel/arch/syscall.h) ───────────────── */

#define SYS_EXIT    0
#define SYS_WRITE   1
#define SYS_READ    2
#define SYS_YIELD   3
#define SYS_GETPID  4
#define SYS_SLEEP   5

/* ── Raw INT 0x80 helpers ─────────────────────────────────────────────── */

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

/* ── Syscall wrappers ─────────────────────────────────────────────────── */

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

/* ── String functions ─────────────────────────────────────────────────── */

int strlen(const char *s) {
    int n = 0;
    while (s[n]) n++;
    return n;
}

int strcmp(const char *a, const char *b) {
    while (*a && *a == *b) { a++; b++; }
    return (unsigned char)*a - (unsigned char)*b;
}

char *strcpy(char *dst, const char *src) {
    char *d = dst;
    while ((*d++ = *src++))
        ;
    return dst;
}

void *memcpy(void *dst, const void *src, size_t n) {
    uint8_t *d = (uint8_t *)dst;
    const uint8_t *s = (const uint8_t *)src;
    while (n--) *d++ = *s++;
    return dst;
}

void *memset(void *dst, int val, size_t n) {
    uint8_t *d = (uint8_t *)dst;
    while (n--) *d++ = (uint8_t)val;
    return dst;
}

/* ── Output helpers ───────────────────────────────────────────────────── */

void puts(const char *s) {
    sys_write(s, strlen(s));
    sys_write("\n", 1);
}

void putchar(char c) {
    sys_write(&c, 1);
}

void print_int(int val) {
    if (val < 0) {
        putchar('-');
        val = -val;
    }
    if (val == 0) {
        putchar('0');
        return;
    }
    char buf[12];
    int i = 0;
    while (val > 0) {
        buf[i++] = '0' + (val % 10);
        val /= 10;
    }
    /* Reverse */
    while (i > 0) {
        putchar(buf[--i]);
    }
}
