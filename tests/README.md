# Tests Directory

Unit test suite for the ApPa kernel. All tests run automatically during boot, after subsystem initialization and before the shell prompt.

## Current Test Results

**47 `[PASS]` assertions across 7 checkpoints at boot, 0 failures.**

Additionally, two test suites run on-demand via shell commands:
- `tasktest` — 6 multitasking tests (Phase 12)
- `usertest` — 3 Ring 3 userspace tests (Phase 13)

## Structure

```
tests/
├── README.md              # This file
├── tests.h                # Master header — includes all test headers
├── tests.c                # Master test runner — dispatches all test suites
├── test_varargs.c/h       # Checkpoint 1: va_list system
├── test_printf.c/h        # Checkpoint 2: kprintf format specifiers (10 groups)
├── test_scroll_log.c/h    # Checkpoint 3: kernel log buffer (currently skipped)
├── test_pmm.c/h           # Checkpoint 4: physical memory manager (15 tests)
├── test_paging.c/h        # Checkpoint 5: paging subsystem (8 tests)
├── test_ata.c/h           # Checkpoint 6: ATA PIO driver (5 tests)
├── test_fs.c/h            # Checkpoint 7: SimpleFS filesystem (10 tests)
├── test_multitask.c/h     # Post-boot: multitasking (6 tests, shell: `tasktest`)
├── test_userspace.c/h     # Post-boot: Ring 3 userspace (3 tests, shell: `usertest`)
└── test_template.c/h.example  # Template for new tests
```

## Test Suites

### Checkpoint 1: `test_varargs` — Variable Arguments
Verifies `va_start`, `va_arg`, `va_end` with mixed types (string, int, string).

### Checkpoint 2: `test_printf` — Formatted Output
10 test groups covering every `kprintf` format specifier:
`%d`, `%u`, `%x`, `%X`, `%c`, `%s`, `%p`, `%%`, mixed formats, edge cases.

### Checkpoint 3: `test_klog` — Kernel Log *(skipped)*
Tests the circular kernel log buffer and log levels. Currently disabled for debugging.

### Checkpoint 4: `test_pmm` — Physical Memory Manager
15 tests: single/multi/contiguous allocation, page alignment, memory accounting, free, double-free detection, unaligned address rejection, out-of-range rejection, zero-alloc, pool range validation, 50-cycle stress test.

### Checkpoint 5: `test_paging` — Paging Subsystem
8 tests: identity map verification (kernel, VGA, heap, PMM pool, boundary), unmapped address detection, dynamic map/unmap round-trip.

### Checkpoint 6: `test_ata` — ATA PIO Driver
5 tests: drive detection, IDENTIFY validation, LBA support check, boot sector signature read, 512-byte write/read round-trip.

### Checkpoint 7: `test_fs` — SimpleFS Filesystem
10 tests: file create, duplicate rejection, write, read+verify, stat, mkdir, directory listing, second file round-trip, delete+verify, nonexistent file error.

### Post-Boot: `test_multitask` — Multitasking (shell: `tasktest`)
6 tests: task creation, context switch verification, preemptive scheduling interleaving, task exit + reap, multi-task concurrent execution, scheduling order analysis. Requires the scheduler to be active; run via `tasktest` shell command.

### Post-Boot: `test_userspace` — Ring 3 Userspace (shell: `usertest`)
3 tests:
1. **SYS_WRITE + SYS_GETPID + SYS_EXIT** — User task prints "Hello from Ring 3!" via INT 0x80, reads its PID, and exits cleanly
2. **SYS_YIELD** — User task cooperatively yields the CPU multiple times
3. **GPF isolation** — User task executes `cli` (privileged instruction) at Ring 3; kernel catches the #GP, kills only that task, system continues

## Running Tests

Boot-time tests execute automatically:

```
kernel_main()
  -> GDT, IDT, PIC, PIT, timer init
  -> kmalloc, PMM, paging init
  -> ATA, RAM disk, SimpleFS init
  -> keyboard, shell init
  -> scheduler init, syscall init
  -> interrupts enabled
  -> run_all_tests()          <- 7 checkpoints run here
  -> scheduler enabled
  -> shell prompt
```

Post-boot tests (require scheduler to be active):

```
> tasktest     <- runs Phase 12 multitasking tests
> usertest     <- runs Phase 13 Ring 3 userspace tests
```

To run headless and inspect results:
```bash
make run-term
# Output goes to stdout and last_run.log
# Ctrl+A then X to exit QEMU
```

Count passes:
```bash
grep -c '\[PASS\]\|\[OK\]' last_run.log
```

## Adding a New Test

### 1. Create the files

**`tests/test_feature.h`**
```c
#ifndef TEST_FEATURE_H
#define TEST_FEATURE_H

void test_feature(void);

#endif
```

**`tests/test_feature.c`**
```c
#include "../drivers/screen.h"
#include "test_feature.h"

void test_feature(void) {
    kprint("=== Testing Feature ===\n");

    // Test 1
    kprint("Test 1: Basic case...\n");
    if (condition) {
        kprint("  [PASS] Basic case\n");
    } else {
        kprint("  [FAIL] Basic case\n");
    }
}
```

### 2. Register in the test runner

**`tests/tests.h`** — add the include:
```c
#include "test_feature.h"
```

**`tests/tests.c`** — add the call with a checkpoint:
```c
kprint("\n>>> Starting test_feature...\n");
test_feature();
kprint(">>> CHECKPOINT N: test_feature COMPLETE\n");
```

### 3. Build and run

```bash
make clean && make run-term
```

The makefile auto-discovers all `.c` files via:
```makefile
C_SOURCES = $(wildcard *.c */*.c */*/*.c)
```

No makefile edits needed.

## Test Guidelines

- **One feature per test function** — keep tests focused and independent
- **Use `[PASS]` / `[FAIL]` tags** — enables automated counting with `grep`
- **Test edge cases** — NULL, zero, max values, invalid inputs
- **No cross-test dependencies** — each test must work in isolation
- **Clean up allocations** — free any pages or heap memory acquired during the test
- **No infinite loops** — tests must terminate

## Disabling Tests

Disable all tests — comment out in `kernel/sys/kernel_main.c`:
```c
// run_all_tests();
```

Disable one test — comment out its call in `tests/tests.c`:
```c
// test_feature();
```

## Debugging Failed Tests

1. Check QEMU serial output for exception messages or triple faults
2. Use `make debug` to step through with GDB
3. Add `kprint()` calls to narrow down the failure point
4. Use `kmalloc_status()` / `pmm_status()` to check for memory corruption
