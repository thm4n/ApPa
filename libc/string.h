#ifndef STRING_H
#define STRING_H

#include "stdint.h"
#include "stddef.h"

// String functions
uint32_t strlen(const char* str);
int strcmp(const char* s1, const char* s2);
char* strcpy(char* dest, const char* src);
char* strncpy(char* dest, const char* src, uint32_t n);

// Memory functions
void* memset(void* ptr, int value, uint32_t num);
void* memcpy(void* dest, const void* src, uint32_t n);

#endif
