# Tests Directory

This directory contains all unit tests for the ApPa kernel.

## Structure

```
tests/
├── README.md           # This file
├── tests.h             # Master header - includes all test headers
├── tests.c             # Master test runner - runs all tests
├── test_varargs.h      # Variable arguments test header
├── test_varargs.c      # Variable arguments test implementation
└── [future tests...]
```

## Running Tests

Tests are automatically run during kernel initialization before entering the main loop.

To run tests:
```bash
make run
```

Tests execute after all kernel subsystems are initialized:
1. IDT initialization
2. PIC remapping
3. Heap initialization
4. Keyboard initialization
5. **→ TESTS RUN HERE ←**
6. System ready (keyboard input loop)

## Adding a New Test

### Step 1: Create Test Files

Create two files in `tests/`:
- `test_yourfeature.h` - Header with function prototype
- `test_yourfeature.c` - Implementation

**Example: `test_yourfeature.h`**
```c
#ifndef TEST_YOURFEATURE_H
#define TEST_YOURFEATURE_H

void test_yourfeature();

#endif
```

**Example: `test_yourfeature.c`**
```c
#include "../drivers/screen.h"
#include "test_yourfeature.h"

void test_yourfeature() {
    kprint("\n=== Testing Your Feature ===\n");
    
    // Your test code here
    // Use kprint() for output
    // Check conditions and report
    
    kprint("  [PASS] Test completed\n");
}
```

### Step 2: Register in Master Test Runner

**Edit `tests/tests.h`:**
```c
#ifndef TESTS_H
#define TESTS_H

#include "test_varargs.h"
#include "test_yourfeature.h"  // Add this line

void run_all_tests();

#endif
```

**Edit `tests/tests.c`:**
```c
void run_all_tests() {
    kprint("\n");
    kprint("=====================================\n");
    kprint("       RUNNING UNIT TESTS\n");
    kprint("=====================================\n");
    
    // Run all tests
    test_va_system();
    test_yourfeature();  // Add this line
    
    kprint("=====================================\n");
    kprint("       ALL TESTS COMPLETED\n");
    kprint("=====================================\n");
    kprint("\n");
}
```

### Step 3: Build and Run

```bash
make clean && make run
```

The makefile automatically picks up all `.c` files in the `tests/` directory via wildcard patterns:
```makefile
C_SOURCES = $(wildcard *.c */*.c)
```

## Test Guidelines

### ✅ DO:
- Test one specific feature per test function
- Use clear, descriptive test names
- Print clear success/failure messages
- Use `kprint()` for output
- Test edge cases (NULL, zero, max values)
- Keep tests simple and focused

### ❌ DON'T:
- Don't make tests depend on each other
- Don't use infinite loops in tests
- Don't allocate memory without freeing it (causes memory leaks)
- Don't crash the kernel (use proper error checking)

## Test Output Format

Use a consistent format for test output:

```c
void test_something() {
    kprint("\n=== Testing Something ===\n");
    
    // Test 1
    kprint("Test 1: Basic functionality\n");
    if (condition) {
        kprint("  [PASS] Basic test passed\n");
    } else {
        kprint("  [FAIL] Basic test failed\n");
    }
    
    // Test 2
    kprint("Test 2: Edge cases\n");
    if (edge_case_works) {
        kprint("  [PASS] Edge case handled\n");
    } else {
        kprint("  [FAIL] Edge case failed\n");
    }
}
```

## Current Tests

### test_varargs.c
**Purpose:** Verify the variable arguments (va_list) system works correctly

**What it tests:**
- `va_start()` initialization
- `va_arg()` for different types (string, int, string)
- `va_end()` cleanup
- Type promotion (char promoted to int)

**Expected output:**
```
=== Testing va_list system ===
First: Hello
Second (int): 42
Third (string): World
```

## Future Test Ideas

- **test_kmalloc.c** - Heap allocator tests
  - Allocate various sizes
  - Free and coalesce
  - Fragmentation scenarios
  - Out-of-memory handling

- **test_string.c** - String utility tests (when implemented)
  - strlen(), strcmp(), strcpy()
  - memset(), memcpy()
  - Edge cases (NULL, empty strings)

- **test_printf.c** - Printf implementation tests
  - All format specifiers (%d, %u, %x, %s, %c, %p)
  - Edge cases (NULL strings, negative numbers)
  - Mixed format strings

- **test_idt.c** - IDT/interrupt tests
  - Verify IDT entries are set correctly
  - Test specific interrupt handlers

- **test_timer.c** - Timer tests (when PIT is implemented)
  - Verify tick counting
  - Uptime calculation

## Disabling Tests

To temporarily disable all tests, comment out the call in `kernel/kernel_main.c`:

```c
// run_all_tests();  // Commented out
```

To disable a specific test, comment it out in `tests/tests.c`:

```c
void run_all_tests() {
    // ...
    test_va_system();
    // test_yourfeature();  // Disabled
    // ...
}
```

## Debugging Failed Tests

If a test causes the kernel to crash:

1. **Check QEMU output** - Look for triple fault or exception messages
2. **Use GDB** - `make debug` for step-by-step debugging
3. **Add more kprint()** - Narrow down where the crash occurs
4. **Check pointers** - NULL pointer dereferences are common
5. **Verify memory** - Use `kmalloc_status()` to check for corruption

## Automated Testing (Future)

Future enhancements:
- Test result tracking (pass/fail count)
- Exit QEMU automatically after tests
- Log output to file for CI/CD
- Assert macros for easier testing
- Mock/stub framework for unit isolation

---

**Remember:** Tests are documentation! Write clear, readable tests that demonstrate how features should work.
