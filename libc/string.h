#ifndef STRING_H
#define STRING_H

#include "stdint.h"
#include "stddef.h"

// String functions
uint32_t strlen(const char* str);
int strcmp(const char* s1, const char* s2);
int strncmp(const char* s1, const char* s2, uint32_t n);
char* strcpy(char* dest, const char* src);
char* strncpy(char* dest, const char* src, uint32_t n);
char* strcat(char* dest, const char* src);

// Conversion functions
void uitoa(uint32_t value, char* str, int base);

// Memory functions
void* memset(void* ptr, int value, uint32_t num);
void* memcpy(void* dest, const void* src, uint32_t n);
int memcmp(const void* s1, const void* s2, uint32_t n);

// Character search functions
char* strchr(const char* str, int c);
char* strrchr(const char* str, int c);

#endif
