/**
 * syscall.c - User-space system call convenience wrappers
 *
 * Each function uses the inline INT 0x80 helpers from syscall.h
 * to invoke the corresponding kernel syscall.
 */

#include "syscall.h"

void sys_exit(void) {
    syscall0(SYS_EXIT);
    /* Should never return, but loop just in case */
    for (;;);
}

int sys_write(const char *buf, int len) {
    return syscall2(SYS_WRITE, (int)buf, len);
}

int sys_read(char *buf, int max) {
    return syscall2(SYS_READ, (int)buf, max);
}

void sys_yield(void) {
    syscall0(SYS_YIELD);
}

int sys_getpid(void) {
    return syscall0(SYS_GETPID);
}

void sys_sleep(int ms) {
    syscall1(SYS_SLEEP, ms);
}
