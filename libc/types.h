/**
 * types.h — Primitive type definitions for user-space programs
 *
 * Provides fixed-width integer types, size_t, and NULL for freestanding
 * Ring 3 programs.  No kernel headers are included.
 */

#ifndef LIBC_TYPES_H
#define LIBC_TYPES_H

/* ── Fixed-width integer types ────────────────────────────────────────────── */

typedef unsigned char   uint8_t;
typedef signed   char   int8_t;
typedef unsigned short  uint16_t;
typedef signed   short  int16_t;
typedef unsigned int    uint32_t;
typedef signed   int    int32_t;

/* ── Size type ────────────────────────────────────────────────────────────── */

typedef unsigned int    size_t;

/* ── Boolean constants ────────────────────────────────────────────────────── */

#define true  1
#define false 0

/* ── NULL pointer ─────────────────────────────────────────────────────────── */

#ifndef NULL
#define NULL ((void *)0)
#endif

#endif /* LIBC_TYPES_H */
