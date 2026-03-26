/**
 * ulibc.h — User-space libc for ApPa
 *
 * Provides types, syscall wrappers, string functions, and output helpers
 * for freestanding Ring 3 programs. Uses INT 0x80 for system calls.
 *
 * User programs include only this header — no kernel headers needed.
 */

#ifndef ULIBC_H
#define ULIBC_H

/* ── Types ────────────────────────────────────────────────────────────────── */

typedef unsigned int    uint32_t;
typedef int             int32_t;
typedef unsigned short  uint16_t;
typedef unsigned char   uint8_t;
typedef unsigned int    size_t;

#define NULL ((void *)0)

/* ── Syscall wrappers ─────────────────────────────────────────────────────── */

/** Terminate the current task with exit code. Does not return. */
void  sys_exit(int code);

/** Write len bytes from buf to the screen. Returns bytes written or -1. */
int   sys_write(const char *buf, int len);

/** Read up to max bytes from keyboard into buf. Returns bytes read or -1. */
int   sys_read(char *buf, int max);

/** Get current task ID. */
int   sys_getpid(void);

/** Voluntarily yield the CPU. */
void  sys_yield(void);

/** Sleep for ms milliseconds. */
void  sys_sleep(int ms);

/* ── String functions ─────────────────────────────────────────────────────── */

int    strlen(const char *s);
int    strcmp(const char *a, const char *b);
char  *strcpy(char *dst, const char *src);
void  *memcpy(void *dst, const void *src, size_t n);
void  *memset(void *dst, int val, size_t n);

/* ── Output helpers ───────────────────────────────────────────────────────── */

/** Write string followed by newline. */
void  puts(const char *s);

/** Write a single character. */
void  putchar(char c);

/** Print decimal integer to screen. */
void  print_int(int val);

#endif /* ULIBC_H */
