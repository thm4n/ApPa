/**
 * stdio.h — User-space standard I/O functions
 *
 * Provides the printf family and basic output helpers for freestanding
 * Ring 3 programs.  All output goes through sys_write() (INT 0x80).
 *
 * ── Implementation roadmap ──────────────────────────────────────────────
 *
 *  Core engine (implement first — everything else builds on it):
 *    vsnprintf   - formatted output into a sized buffer with va_list
 *
 *  Built on top of vsnprintf:
 *    snprintf    - like vsnprintf but takes ...  instead of va_list
 *    vsprintf    - vsnprintf with unlimited size (size = SIZE_MAX)
 *    sprintf     - vsprintf  but takes ...  instead of va_list
 *    vprintf     - vsnprintf → sys_write
 *    printf      - vprintf   but takes ...  instead of va_list
 *
 *  Number conversion helpers (used internally by vsnprintf):
 *    itoa        - signed   int → string in any base (2–36)
 *    utoa        - unsigned int → string in any base (2–36)
 *
 *  Format specifiers to support:
 *    %d / %i     signed decimal          (int)
 *    %u          unsigned decimal         (unsigned int)
 *    %x          lowercase hexadecimal    (unsigned int)
 *    %X          uppercase hexadecimal    (unsigned int)
 *    %o          octal                    (unsigned int)
 *    %c          single character         (int → char)
 *    %s          NUL-terminated string    (const char *)
 *    %p          pointer as 0xHEX        (void *)
 *    %%          literal '%'
 *
 *  Optional width / flags (stretch goals):
 *    -           left-justify
 *    0           zero-pad
 *    [width]     minimum field width
 *    .[prec]     precision (string truncation / min digits)
 *    l           long modifier (for %ld, %lu, %lx, etc.)
 * ────────────────────────────────────────────────────────────────────────
 */

#ifndef LIBC_STDIO_H
#define LIBC_STDIO_H

#include "types.h"
#include "stdarg.h"

/* ── Number conversion helpers ────────────────────────────────────────────
 *
 * Convert an integer to a NUL-terminated string in the given base.
 * @str must be large enough to hold the result (33 bytes covers base-2
 * for a 32-bit value + sign + NUL).
 *
 * Supported bases: 2 – 36.  Digits beyond 9 use lowercase a–z.
 * itoa() emits a leading '-' for negative values in base 10 only.
 */

/** Signed integer → string. */
void  itoa(int32_t value, char *str, int base);

/** Unsigned integer → string. */
void  utoa(uint32_t value, char *str, int base);

/* ── printf family ────────────────────────────────────────────────────────
 *
 * All functions return the number of characters that *would* have been
 * written (excluding the NUL terminator), even if the output was
 * truncated (matching C99 snprintf semantics).
 *
 * Implementation order recommendation:
 *   1) vsnprintf  (core formatter — everything delegates here)
 *   2) snprintf   (thin ... wrapper around vsnprintf)
 *   3) vsprintf   (calls vsnprintf with SIZE_MAX)
 *   4) sprintf    (thin ... wrapper around vsprintf)
 *   5) vprintf    (vsnprintf into a local buffer → sys_write)
 *   6) printf     (thin ... wrapper around vprintf)
 */

/**
 * vsnprintf — Core formatted output into a sized buffer.
 *
 * Writes at most @size - 1 characters into @buf followed by a NUL.
 * If @size is 0, nothing is written (but the return value still
 * reports how many characters would have been needed).
 *
 * @buf    Destination buffer (may be NULL when @size is 0).
 * @size   Total size of @buf in bytes.
 * @fmt    printf-style format string.
 * @args   Variable argument list.
 * @return Number of characters that would have been written (excl. NUL).
 */
int   vsnprintf(char *buf, size_t size, const char *fmt, va_list args);

/**
 * snprintf — Formatted output into a sized buffer.
 *
 * Identical to vsnprintf but accepts variadic arguments directly.
 */
int   snprintf(char *buf, size_t size, const char *fmt, ...);

/**
 * vsprintf — Formatted output into a buffer (unbounded).
 *
 * Equivalent to vsnprintf(buf, SIZE_MAX, fmt, args).
 * WARNING: No overflow protection — caller must ensure @buf is large enough.
 */
int   vsprintf(char *buf, const char *fmt, va_list args);

/**
 * sprintf — Formatted output into a buffer (unbounded).
 *
 * Variadic wrapper around vsprintf.
 * WARNING: No overflow protection.
 */
int   sprintf(char *buf, const char *fmt, ...);

/**
 * vprintf — Formatted output to the console via sys_write (va_list).
 *
 * Formats into an internal buffer then calls sys_write() once.
 * @return Number of characters written.
 */
int   vprintf(const char *fmt, va_list args);

/**
 * printf — Formatted output to the console via sys_write.
 *
 * @return Number of characters written, or -1 on error.
 */
int   printf(const char *fmt, ...);

/* ── Legacy output helpers (existing API) ─────────────────────────────────
 *
 * These predate the printf family and remain available for convenience.
 * They can be reimplemented on top of printf once it's working, or kept
 * as lightweight alternatives.
 */

/** Write string @s followed by a newline.  Returns bytes written. */
int   puts(const char *s);

/** Write a single character @c to the screen. */
void  putchar(char c);

/** Print a decimal integer to the screen. */
void  print_int(int val);

/** Print an unsigned 32-bit integer in hexadecimal (with 0x prefix). */
void  print_hex(unsigned int val);

#endif /* LIBC_STDIO_H */
