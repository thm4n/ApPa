/**
 * stdio.h — User-space standard I/O functions
 *
 * Provides basic output helpers for freestanding Ring 3 programs.
 * Uses sys_write() (INT 0x80) under the hood.
 */

#ifndef LIBC_STDIO_H
#define LIBC_STDIO_H

/* ── Output functions ─────────────────────────────────────────────────────── */

/** Write string @s followed by a newline.  Returns bytes written. */
int   puts(const char *s);

/** Write a single character @c to the screen. */
void  putchar(char c);

/** Print a decimal integer to the screen. */
void  print_int(int val);

/** Print an unsigned 32-bit integer in hexadecimal (with 0x prefix). */
void  print_hex(unsigned int val);

#endif /* LIBC_STDIO_H */
