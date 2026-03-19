# Variable Arguments (va_list) System - Technical Reference

## What We Implemented

✅ **File:** [libc/stdarg.h](../libc/stdarg.h)  
✅ **Test:** [kernel/test_varargs.c](../kernel/test_varargs.c)  
✅ **Status:** Working and tested

---

## The Four Components

### 1. `va_list` - The Argument Pointer

```c
typedef __builtin_va_list va_list;
```

**What it is:** A special pointer type that tracks the current position in the variable arguments.

**Think of it as:** A cursor moving through the arguments on the stack.

### 2. `va_start(ap, last)` - Initialize

```c
#define va_start(ap, last) __builtin_va_start(ap, last)
```

**What it does:** Points `ap` to the first variable argument (right after the `last` named parameter).

**Example:**
```c
void func(const char* format, ...) {
    va_list args;
    va_start(args, format);  // Now 'args' points to first arg after 'format'
```

### 3. `va_arg(ap, type)` - Get Next Argument

```c
#define va_arg(ap, type) __builtin_va_arg(ap, type)
```

**What it does:**
1. Reads the current argument as the specified `type`
2. Advances the pointer to the next argument
3. Returns the value

**Example:**
```c
    int value = va_arg(args, int);      // Get an int
    char* str = va_arg(args, char*);    // Get a string pointer
```

### 4. `va_end(ap)` - Cleanup

```c
#define va_end(ap) __builtin_va_end(ap)
```

**What it does:** Cleans up the `va_list` (often a no-op, but good practice).

**Example:**
```c
    va_end(args);  // Always call this when done
}
```

---

## Complete Example

```c
#include "../libc/stdarg.h"
#include "../drivers/screen.h"

void my_printf(const char* format, ...) {
    va_list args;                    // 1. Declare va_list
    va_start(args, format);          // 2. Initialize (point to first vararg)
    
    // Simple example: print format, then one int, then one string
    kprint(format);
    kprint("\n");
    
    int num = va_arg(args, int);     // 3. Get first argument (int)
    kprint("Number: ");
    kprint_uint(num);
    kprint("\n");
    
    char* str = va_arg(args, char*); // 4. Get second argument (string)
    kprint("String: ");
    kprint(str);
    kprint("\n");
    
    va_end(args);                    // 5. Cleanup
}

// Usage:
my_printf("Hello", 42, "World");
// Output:
// Hello
// Number: 42
// String: World
```

---

## How It Works Under the Hood (i686)

### Memory Layout When Calling `func("fmt", 10, "hi")`

```
Stack grows DOWN (from high to low addresses)
                                    
┌──────────────────┐ ← Higher addresses
│    "hi"          │ arg2 (char*)
├──────────────────┤
│     10           │ arg1 (int)
├──────────────────┤
│    "fmt"         │ format parameter (last named param)
├──────────────────┤
│  return address  │ ← Pushed by CALL instruction
├──────────────────┤
│   old EBP        │ ← Saved frame pointer
├──────────────────┤
│  local vars      │ ← Function's stack frame
└──────────────────┘ ← Lower addresses
     ESP points here
```

### What Each Macro Does:

```c
void func(const char* format, ...) {
    va_list args;
    
    // va_start(args, format)
    // → Sets args to point to address right after 'format'
    // → Now args points to where '10' is stored
    
    va_start(args, format);
    
    // va_arg(args, int)
    // → Read 4 bytes at current location as int (get 10)
    // → Advance args by sizeof(int) 
    // → Now args points to where "hi" is stored
    
    int first = va_arg(args, int);    // first = 10
    
    // va_arg(args, char*)
    // → Read 4 bytes at current location as pointer
    // → Advance args by sizeof(char*)
    
    char* second = va_arg(args, char*);  // second = "hi"
    
    va_end(args);  // Cleanup (no-op on i686)
}
```

---

## Critical Type Promotion Rules ⚠️

C **automatically promotes** small types when passed as varargs:

```c
// Type promotion rules:
char   → int       (promoted)
short  → int       (promoted)
float  → double    (promoted)

// Why promotion happens:
// - Allows printf-like functions to work efficiently
// - Ensures minimum argument size (word-aligned)
// - Historical compatibility
```

### ❌ Common Mistake:

```c
void print_char(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    
    char c = va_arg(args, char);  // ❌ WRONG! Will crash or get garbage
    
    va_end(args);
}

// When you call:
print_char("X", 'A');
// 'A' is promoted to int on stack (4 bytes, not 1)
```

### ✅ Correct Way:

```c
void print_char(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    
    char c = (char)va_arg(args, int);  // ✅ Correct! Read as int, cast to char
    
    va_end(args);
}
```

---

## Why Use GCC Builtins?

### Alternative: Manual Implementation (DON'T USE)

```c
// Manual implementation (i686 specific, error-prone)
typedef char* va_list;
#define va_start(ap, last) (ap = (char*)&last + sizeof(last))
#define va_arg(ap, type) (*(type*)((ap += sizeof(type)) - sizeof(type)))
#define va_end(ap) (ap = NULL)
```

**Problems with manual implementation:**
- ❌ Architecture-specific (breaks on x86_64, ARM)
- ❌ Doesn't handle alignment correctly
- ❌ Doesn't handle type promotion properly
- ❌ Breaks with different calling conventions
- ❌ May fail with optimization flags

### GCC Builtins (RECOMMENDED)

```c
// Simple, portable, reliable
typedef __builtin_va_list va_list;
#define va_start(ap, last) __builtin_va_start(ap, last)
#define va_arg(ap, type) __builtin_va_arg(ap, type)
#define va_end(ap) __builtin_va_end(ap)
```

**Benefits:**
- ✅ Works on ALL architectures (i686, x86_64, ARM, RISC-V)
- ✅ Handles alignment automatically
- ✅ Optimized by compiler
- ✅ Type-safe
- ✅ Compatible with all calling conventions
- ✅ Only 4 lines of code!

---

## Testing Your Implementation

### Test File: [kernel/test_varargs.c](../kernel/test_varargs.c)

```c
#include "../libc/stdarg.h"
#include "../drivers/screen.h"

void test_varargs(const char* first, ...) {
    va_list args;
    va_start(args, first);
    
    // Test string
    kprint("First: ");
    kprint((char*)first);
    kprint("\n");
    
    // Test int
    int second = va_arg(args, int);
    kprint("Second (int): ");
    kprint_uint(second);
    kprint("\n");
    
    // Test another string
    char* third = va_arg(args, char*);
    kprint("Third (string): ");
    kprint(third);
    kprint("\n");
    
    va_end(args);
}

void test_va_system() {
    kprint("\n=== Testing va_list system ===\n");
    test_varargs("Hello", 42, "World");
}
```

**Expected Output:**
```
=== Testing va_list system ===
First: Hello
Second (int): 42
Third (string): World
```

---

## Using va_list for printf

Now that you have `va_list`, you can implement `kprintf()`:

```c
#include "../libc/stdarg.h"
#include "../drivers/screen.h"

void kprintf(const char* format, ...) {
    va_list args;
    va_start(args, format);
    
    for (int i = 0; format[i] != '\0'; i++) {
        if (format[i] == '%' && format[i+1] != '\0') {
            i++;  // Move to specifier
            
            switch (format[i]) {
                case 'd': {  // Integer
                    int value = va_arg(args, int);
                    // Convert to string and print
                    char buffer[32];
                    itoa(value, buffer, 10);
                    kprint(buffer);
                    break;
                }
                
                case 's': {  // String
                    char* str = va_arg(args, char*);
                    kprint(str ? str : "(null)");
                    break;
                }
                
                case 'c': {  // Character
                    char c = (char)va_arg(args, int);  // Note: read as int!
                    char str[2] = {c, '\0'};
                    kprint(str);
                    break;
                }
                
                // ... more specifiers
            }
        } else {
            // Regular character
            char str[2] = {format[i], '\0'};
            kprint(str);
        }
    }
    
    va_end(args);
}

// Usage:
kprintf("Value: %d, String: %s, Char: %c\n", 42, "Hello", 'X');
```

---

## Common Pitfalls

### 1. Reading Wrong Type

```c
// WRONG:
char c = va_arg(args, char);     // ❌ char is promoted to int!

// CORRECT:
char c = (char)va_arg(args, int); // ✅ Read as int, cast to char
```

### 2. Forgetting va_end()

```c
void func(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    
    int x = va_arg(args, int);
    
    // ❌ Forgot va_end(args)!
}  // Can cause resource leaks on some platforms
```

### 3. Calling va_arg() Too Many Times

```c
void func(int count, ...) {
    va_list args;
    va_start(args, count);
    
    for (int i = 0; i < 10; i++) {  // ❌ User only passed 3 args!
        int x = va_arg(args, int);  // Will read garbage/crash after 3rd
    }
    
    va_end(args);
}
```

### 4. Not Checking for NULL Strings

```c
case 's': {
    char* str = va_arg(args, char*);
    kprint(str);  // ❌ Crashes if str is NULL!
}

// CORRECT:
case 's': {
    char* str = va_arg(args, char*);
    kprint(str ? str : "(null)");  // ✅ Safe
}
```

---

## Summary

✅ **Created:** `libc/stdarg.h` with GCC builtins  
✅ **Tested:** Working variable arguments system  
✅ **Ready for:** Implementing `kprintf()` and other variadic functions

**Next Step:** Implement string utilities and number conversion functions, then build `kprintf()` on top of this foundation!

---

## References

- **GCC Documentation:** [Variadic Functions](https://gcc.gnu.org/onlinedocs/gcc/Other-Builtins.html)
- **C Standard:** ISO/IEC 9899 Section 7.16 (stdarg.h)
- **x86 Calling Convention:** [System V ABI](https://wiki.osdev.org/System_V_ABI)
