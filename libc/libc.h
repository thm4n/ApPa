/**
 * libc.h — Umbrella header for ApPa user-space C library
 *
 * Including this single header gives user programs access to all
 * libc facilities: types, syscalls, strings, and I/O.
 *
 * Individual headers can also be included directly for finer control.
 */

#ifndef LIBC_H
#define LIBC_H

#include "types.h"
#include "syscall.h"
#include "string.h"
#include "stdio.h"

#endif /* LIBC_H */
