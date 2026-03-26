/**
 * stdarg.h — Variable argument macros for user-space programs
 *
 * Wraps GCC's built-in va_list support.  Works in freestanding mode
 * without any standard library dependency.
 */

#ifndef LIBC_STDARG_H
#define LIBC_STDARG_H

typedef __builtin_va_list va_list;

#define va_start(ap, last)  __builtin_va_start(ap, last)
#define va_arg(ap, type)    __builtin_va_arg(ap, type)
#define va_end(ap)          __builtin_va_end(ap)
#define va_copy(dst, src)   __builtin_va_copy(dst, src)

#endif /* LIBC_STDARG_H */
