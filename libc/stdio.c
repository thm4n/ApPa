/**
 * stdio.c — User-space standard I/O implementation
 *
 * Uses sys_write() from syscall.h for all output.
 * Pure freestanding C — no kernel headers.
 */

#include "stdio.h"
#include "string.h"
#include "syscall.h"

/* ── Output functions ─────────────────────────────────────────────────────── */

int puts(const char *s) {
    int n = sys_write(s, strlen(s));
    sys_write("\n", 1);
    return n + 1;
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

void print_hex(unsigned int val) {
    static const char hex[] = "0123456789abcdef";
    char buf[11];   /* "0x" + 8 hex digits + NUL */
    buf[0] = '0';
    buf[1] = 'x';

    /* Fill 8 hex digits (zero-padded) */
    for (int i = 7; i >= 0; i--) {
        buf[2 + i] = hex[val & 0xF];
        val >>= 4;
    }
    buf[10] = '\0';
    sys_write(buf, 10);
}
