#ifndef STDIO_H
#define STDIO_H

#include "stdint.h"
#include "stdarg.h"

// Number conversion functions
void itoa(int32_t value, char* str, int base);
void utoa(uint32_t value, char* str, int base);

// Printf implementation
void kprintf(const char* format, ...);

#endif
