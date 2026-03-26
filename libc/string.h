/**
 * string.h — User-space string and memory functions
 *
 * Standard-like string utilities for freestanding Ring 3 programs.
 * Implemented in pure C with no kernel dependencies.
 */

#ifndef LIBC_STRING_H
#define LIBC_STRING_H

#include "types.h"

/* ── String functions ─────────────────────────────────────────────────────── */

/** Return the length of @s (not counting the NUL terminator). */
int    strlen(const char *s);

/** Compare two strings lexicographically.  Returns <0, 0, or >0. */
int    strcmp(const char *a, const char *b);

/** Compare at most @n characters of two strings. */
int    strncmp(const char *a, const char *b, size_t n);

/** Copy @src to @dst (including NUL).  Returns @dst. */
char  *strcpy(char *dst, const char *src);

/** Copy at most @n characters from @src to @dst.  Pads with NUL. Returns @dst. */
char  *strncpy(char *dst, const char *src, size_t n);

/* ── Memory functions ─────────────────────────────────────────────────────── */

/** Copy @n bytes from @src to @dst (no overlap).  Returns @dst. */
void  *memcpy(void *dst, const void *src, size_t n);

/** Fill @n bytes at @dst with byte value @val.  Returns @dst. */
void  *memset(void *dst, int val, size_t n);

/** Compare @n bytes.  Returns <0, 0, or >0. */
int    memcmp(const void *a, const void *b, size_t n);

#endif /* LIBC_STRING_H */
