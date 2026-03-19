# Printf Implementation Plan

## Overview
Implement a `kprintf()` function that supports formatted string output, similar to standard C `printf()`. This will greatly improve debugging and output flexibility throughout the kernel.

**Goal:** `kprintf("Value: %d, Hex: 0x%x, String: %s\n", 42, 0xDEAD, "Hello");`

---

## Prerequisites Assessment

### ✅ Already Have:
- `kprint()` - Basic string output
- `kprint_hex()` - Hexadecimal output
- `kprint_uint()` - Unsigned integer output
- VGA text mode screen driver
- Character-by-character output capability

### ❌ Need to Implement:
- Variable argument handling (va_list, va_start, va_arg, va_end)
- Format string parser
- Generic number-to-string conversion
- Format specifier handlers
- Main kprintf() function

---

## Mission List

### Mission 1: Create stdarg.h for Variable Arguments ✅ COMPLETED

**Priority:** Critical | **Estimated Time:** 30 minutes  
**Why:** Required for handling variable number of arguments to printf

| Task | File | Description | Status |
|------|------|-------------|--------|
| 1.1 | `libc/stdarg.h` | Create header file | ✅ |
| 1.2 | `libc/stdarg.h` | Define `va_list` type | ✅ |
| 1.3 | `libc/stdarg.h` | Define `va_start()` macro | ✅ |
| 1.4 | `libc/stdarg.h` | Define `va_arg()` macro | ✅ |
| 1.5 | `libc/stdarg.h` | Define `va_end()` macro | ✅ |

**Implementation:**
```c
#ifndef STDARG_H
#define STDARG_H

typedef __builtin_va_list va_list;
#define va_start(ap, last) __builtin_va_start(ap, last)
#define va_arg(ap, type) __builtin_va_arg(ap, type)
#define va_end(ap) __builtin_va_end(ap)

#endif
```

**Why use GCC builtins?**
- Cross-compiler compatible
- Handles all architecture-specific details
- Reliable and well-tested
- No manual stack manipulation needed

---

### Mission 2: Create String Utility Functions ⬜ NOT STARTED

**Priority:** High | **Estimated Time:** 1 hour  
**Why:** Printf needs string length, comparison, and manipulation

| Task | File | Description | Status |
|------|------|-------------|--------|
| 2.1 | `libc/string.h` | Create header file | ⬜ |
| 2.2 | `libc/string.c` | Create source file | ⬜ |
| 2.3 | `libc/string.c` | Implement `strlen()` - get string length | ⬜ |
| 2.4 | `libc/string.c` | Implement `strcmp()` - compare strings | ⬜ |
| 2.5 | `libc/string.c` | Implement `strcpy()` - copy strings | ⬜ |
| 2.6 | `libc/string.c` | Implement `strncpy()` - copy n characters | ⬜ |
| 2.7 | `libc/string.c` | Implement `memset()` - set memory bytes | ⬜ |
| 2.8 | `libc/string.c` | Implement `memcpy()` - copy memory | ⬜ |

**Implementation Examples:**
```c
// libc/string.h
#ifndef STRING_H
#define STRING_H

#include "stdint.h"
#include "stddef.h"

uint32_t strlen(const char* str);
int strcmp(const char* s1, const char* s2);
char* strcpy(char* dest, const char* src);
char* strncpy(char* dest, const char* src, uint32_t n);
void* memset(void* ptr, int value, uint32_t num);
void* memcpy(void* dest, const void* src, uint32_t n);

#endif
```

```c
// libc/string.c
#include "string.h"

uint32_t strlen(const char* str) {
    uint32_t len = 0;
    while (str[len])
        len++;
    return len;
}

// ... other implementations
```

---

### Mission 3: Create Number Conversion Functions ⬜ NOT STARTED

**Priority:** High | **Estimated Time:** 1.5 hours  
**Why:** Convert integers to strings for %d, %u, %x, %o format specifiers

| Task | File | Description | Status |
|------|------|-------------|--------|
| 3.1 | `libc/stdio.h` | Create header file | ⬜ |
| 3.2 | `libc/stdio.c` | Create source file | ⬜ |
| 3.3 | `libc/stdio.c` | Implement `itoa()` - integer to string (base 10) | ⬜ |
| 3.4 | `libc/stdio.c` | Implement `utoa()` - unsigned to string | ⬜ |
| 3.5 | `libc/stdio.c` | Implement `itoa_base()` - any base (2-36) | ⬜ |
| 3.6 | `libc/stdio.c` | Implement reverse string helper | ⬜ |

**Implementation:**
```c
// libc/stdio.h
#ifndef STDIO_H
#define STDIO_H

#include "stdint.h"

void itoa(int32_t value, char* str, int base);
void utoa(uint32_t value, char* str, int base);

#endif
```

```c
// libc/stdio.c
#include "stdio.h"

static void reverse(char* str, int length) {
    int start = 0;
    int end = length - 1;
    while (start < end) {
        char temp = str[start];
        str[start] = str[end];
        str[end] = temp;
        start++;
        end--;
    }
}

void utoa(uint32_t value, char* str, int base) {
    int i = 0;
    
    if (value == 0) {
        str[i++] = '0';
        str[i] = '\0';
        return;
    }
    
    while (value != 0) {
        int rem = value % base;
        str[i++] = (rem > 9) ? (rem - 10) + 'a' : rem + '0';
        value = value / base;
    }
    
    str[i] = '\0';
    reverse(str, i);
}

void itoa(int32_t value, char* str, int base) {
    int i = 0;
    int is_negative = 0;
    
    if (value == 0) {
        str[i++] = '0';
        str[i] = '\0';
        return;
    }
    
    // Handle negative numbers (only for base 10)
    if (value < 0 && base == 10) {
        is_negative = 1;
        value = -value;
    }
    
    // Process digits
    while (value != 0) {
        int rem = value % base;
        str[i++] = (rem > 9) ? (rem - 10) + 'a' : rem + '0';
        value = value / base;
    }
    
    // Add negative sign
    if (is_negative)
        str[i++] = '-';
    
    str[i] = '\0';
    reverse(str, i);
}
```

---

### Mission 4: Implement kprintf() Core Function ⬜ NOT STARTED

**Priority:** Critical | **Estimated Time:** 2-3 hours  
**Why:** The main printf implementation with format parsing

| Task | File | Description | Status |
|------|------|-------------|--------|
| 4.1 | `libc/stdio.h` | Add `kprintf()` prototype | ⬜ |
| 4.2 | `libc/stdio.c` | Implement format string parser | ⬜ |
| 4.3 | `libc/stdio.c` | Handle `%d` - signed decimal | ⬜ |
| 4.4 | `libc/stdio.c` | Handle `%u` - unsigned decimal | ⬜ |
| 4.5 | `libc/stdio.c` | Handle `%x` - lowercase hex | ⬜ |
| 4.6 | `libc/stdio.c` | Handle `%X` - uppercase hex | ⬜ |
| 4.7 | `libc/stdio.c` | Handle `%c` - character | ⬜ |
| 4.8 | `libc/stdio.c` | Handle `%s` - string | ⬜ |
| 4.9 | `libc/stdio.c` | Handle `%p` - pointer (hex address) | ⬜ |
| 4.10 | `libc/stdio.c` | Handle `%%` - literal percent | ⬜ |

**Implementation:**
```c
// libc/stdio.h
void kprintf(const char* format, ...);

// libc/stdio.c
#include "stdio.h"
#include "stdarg.h"
#include "../drivers/screen.h"

void kprintf(const char* format, ...) {
    va_list args;
    va_start(args, format);
    
    char buffer[32];  // Buffer for number conversions
    
    for (int i = 0; format[i] != '\0'; i++) {
        if (format[i] == '%') {
            i++;  // Move to format specifier
            
            switch (format[i]) {
                case 'd':  // Signed decimal
                case 'i': {
                    int val = va_arg(args, int);
                    itoa(val, buffer, 10);
                    kprint(buffer);
                    break;
                }
                
                case 'u': {  // Unsigned decimal
                    unsigned int val = va_arg(args, unsigned int);
                    utoa(val, buffer, 10);
                    kprint(buffer);
                    break;
                }
                
                case 'x': {  // Lowercase hex
                    unsigned int val = va_arg(args, unsigned int);
                    utoa(val, buffer, 16);
                    kprint(buffer);
                    break;
                }
                
                case 'X': {  // Uppercase hex
                    unsigned int val = va_arg(args, unsigned int);
                    utoa(val, buffer, 16);
                    // Convert to uppercase
                    for (int j = 0; buffer[j]; j++) {
                        if (buffer[j] >= 'a' && buffer[j] <= 'f')
                            buffer[j] -= 32;
                    }
                    kprint(buffer);
                    break;
                }
                
                case 'c': {  // Character
                    char c = (char)va_arg(args, int);
                    char str[2] = {c, '\0'};
                    kprint(str);
                    break;
                }
                
                case 's': {  // String
                    char* str = va_arg(args, char*);
                    if (str == NULL)
                        kprint("(null)");
                    else
                        kprint(str);
                    break;
                }
                
                case 'p': {  // Pointer
                    void* ptr = va_arg(args, void*);
                    kprint("0x");
                    utoa((unsigned int)ptr, buffer, 16);
                    kprint(buffer);
                    break;
                }
                
                case '%': {  // Literal %
                    char str[2] = {'%', '\0'};
                    kprint(str);
                    break;
                }
                
                default: {  // Unknown specifier
                    char str[3] = {'%', format[i], '\0'};
                    kprint(str);
                    break;
                }
            }
        } else {
            // Regular character
            char str[2] = {format[i], '\0'};
            kprint(str);
        }
    }
    
    va_end(args);
}
```

---

### Mission 5: Testing & Integration ⬜ NOT STARTED

**Priority:** High | **Estimated Time:** 1 hour  
**Why:** Verify all format specifiers work correctly

| Task | File | Description | Status |
|------|------|-------------|--------|
| 5.1 | `makefile` | Add `libc/string.c` and `libc/stdio.c` to build | ⬜ |
| 5.2 | `kernel/kernel_main.c` | Add test code with various format specifiers | ⬜ |
| 5.3 | Test | Test `%d` with positive, negative, zero | ⬜ |
| 5.4 | Test | Test `%u` with large unsigned values | ⬜ |
| 5.5 | Test | Test `%x` and `%X` with hex values | ⬜ |
| 5.6 | Test | Test `%c` with various characters | ⬜ |
| 5.7 | Test | Test `%s` with strings and NULL | ⬜ |
| 5.8 | Test | Test `%p` with pointers | ⬜ |
| 5.9 | Test | Test `%%` for literal percent | ⬜ |
| 5.10 | Test | Test mixed format strings | ⬜ |

**Test Code Examples:**
```c
void test_printf() {
    kprintf("\n=== Testing kprintf ===\n");
    
    // Test integers
    kprintf("Signed: %d, %d, %d\n", 42, -42, 0);
    kprintf("Unsigned: %u, %u\n", 42, 4294967295);
    
    // Test hex
    kprintf("Hex lowercase: %x\n", 0xDEADBEEF);
    kprintf("Hex uppercase: %X\n", 0xDEADBEEF);
    
    // Test char
    kprintf("Char: %c %c %c\n", 'A', 'B', 'C');
    
    // Test string
    kprintf("String: %s\n", "Hello, World!");
    kprintf("Null string: %s\n", NULL);
    
    // Test pointer
    int x = 42;
    kprintf("Pointer: %p\n", &x);
    
    // Test percent
    kprintf("Percent: 100%%\n");
    
    // Test mixed
    kprintf("Mixed: %s has value %d (0x%x) at %p\n", 
            "Variable", 42, 42, &x);
}
```

---

### Mission 6: Replace Old Print Functions ⬜ NOT STARTED

**Priority:** Medium | **Estimated Time:** 30 minutes  
**Why:** Simplify codebase by using kprintf everywhere

| Task | File | Description | Status |
|------|------|-------------|--------|
| 6.1 | Replace throughout | Change `kprint()` + `kprint_uint()` to `kprintf()` | ⬜ |
| 6.2 | Replace throughout | Change `kprint()` + `kprint_hex()` to `kprintf()` | ⬜ |
| 6.3 | `kernel/kmalloc.c` | Update `kmalloc_status()` to use kprintf | ⬜ |
| 6.4 | Various | Replace verbose print sequences | ⬜ |

**Before:**
```c
kprint("Total blocks: ");
kprint_uint(total_blocks);
kprint("\n");
```

**After:**
```c
kprintf("Total blocks: %u\n", total_blocks);
```

---

## Optional Enhancements (Future)

### Enhancement 1: Width and Precision Support
```c
kprintf("%5d", 42);      // "   42" (width 5)
kprintf("%.2d", 5);      // "05" (precision 2)
kprintf("%10s", "Hi");   // "        Hi" (width 10)
```

### Enhancement 2: Additional Format Specifiers
- `%o` - Octal
- `%b` - Binary
- `%f` - Float (requires float support)
- `%ld` - Long integers

### Enhancement 3: sprintf() for String Formatting
```c
char buffer[100];
sprintf(buffer, "Value: %d", 42);
// buffer now contains "Value: 42"
```

### Enhancement 4: snprintf() for Safe Buffer Operations
```c
char buffer[10];
snprintf(buffer, sizeof(buffer), "Long string...");
// Prevents buffer overflow
```

---

## File Structure After Completion

```
libc/
    stdarg.h       # NEW - Variable arguments
    string.h       # NEW - String utilities header
    string.c       # NEW - String utilities implementation
    stdio.h        # NEW - Printf header
    stdio.c        # NEW - Printf implementation
    stdint.h       # Existing
    stddef.h       # Existing

kernel/
    kernel_main.c  # MODIFIED - Use kprintf
    kmalloc.c      # MODIFIED - Use kprintf in status
    
drivers/
    screen.h       # Keep existing (kprintf uses kprint internally)
    screen.c       # Keep existing
```

---

## Execution Order

```
1. Create libc/stdarg.h (Mission 1)
2. Create libc/string.h + libc/string.c (Mission 2)
3. Create libc/stdio.h + libc/stdio.c with conversion functions (Mission 3)
4. Implement kprintf() in libc/stdio.c (Mission 4)
5. Update makefile (Mission 5)
6. Test all format specifiers (Mission 5)
7. Gradually replace old print functions (Mission 6)
```

---

## Common Pitfalls & Solutions

### 1. **va_arg Type Promotion**
**Problem:** Small types (char, short) are promoted to int when passed as varargs.

**Solution:** Always use `va_arg(args, int)` then cast:
```c
char c = (char)va_arg(args, int);  // Correct
char c = va_arg(args, char);       // WRONG!
```

### 2. **Format String Not Null-Terminated**
**Problem:** Crashes if format string is not properly terminated.

**Solution:** Always ensure format strings have `\0`:
```c
kprintf("Test\n");  // Correct (string literals auto-terminated)
```

### 3. **Number Buffer Too Small**
**Problem:** Overflow when converting large numbers to strings.

**Solution:** Use adequate buffer size (32 bytes is safe for 32-bit ints in any base).

### 4. **Forgetting va_end()**
**Problem:** Memory/resource leak on some platforms.

**Solution:** Always call `va_end()` even though it's often a no-op with GCC builtins.

### 5. **NULL String Pointer**
**Problem:** Crashes when printing NULL string.

**Solution:** Check for NULL and print "(null)":
```c
if (str == NULL)
    kprint("(null)");
```

---

## Verification Checklist

- [ ] All format specifiers work (%d, %u, %x, %X, %c, %s, %p, %%)
- [ ] Negative numbers display correctly
- [ ] Zero values handled properly
- [ ] NULL strings don't crash
- [ ] Hex values display correctly (both cases)
- [ ] Pointers show as 0x... addresses
- [ ] Mixed format strings work
- [ ] No buffer overflows
- [ ] Build completes without warnings

---

## Benefits After Implementation

✅ **Cleaner code:** One function instead of multiple  
✅ **Easier debugging:** Format complex output easily  
✅ **More readable:** `kprintf("x=%d y=%d", x, y)` vs multiple kprint calls  
✅ **Standard-like:** Familiar to C programmers  
✅ **Future-proof:** Easy to extend with more format specifiers  

---

## Estimated Total Time

- Mission 1: 30 minutes
- Mission 2: 1 hour
- Mission 3: 1.5 hours
- Mission 4: 2-3 hours
- Mission 5: 1 hour
- Mission 6: 30 minutes

**Total: 6-7 hours** (can be done in one day)
