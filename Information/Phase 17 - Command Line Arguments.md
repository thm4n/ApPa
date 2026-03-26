# Phase 17 — Command-Line Arguments, crt0 & User Libc

## Goal

1. Pass command-line arguments from the shell's `exec` command to user-space
   ELF programs, so `exec hello.elf foo bar` delivers `argc=3` and
   `argv={"hello.elf", "foo", "bar"}` to `_start`.

2. Provide a **crt0** (C runtime zero) startup stub so user programs can
   be written with a standard `int main(int argc, char **argv)` entry point
   instead of hand-coding `_start` with inline assembly.

3. Provide a **user-space libc** with syscall wrappers and string utilities,
   so user programs don't have to inline every helper and can simply
   `#include "ulibc.h"`.

---

## Theory — How argv/argc Works on x86

### The Unix Convention

On Unix/Linux, when the kernel loads a new process, it places the argument
data on the **user stack** before jumping to the entry point. The entry
point (`_start`) finds `argc` at the top of the stack, followed by an
array of `char *` pointers (argv), followed by a NULL sentinel, and then
the actual string bytes somewhere below.

```
High address (stack bottom)
┌──────────────────────────────────┐
│  "bar\0"                         │  String data (copied bytes)
│  "foo\0"                         │
│  "hello.elf\0"                   │
├──────────────────────────────────┤
│  NULL              (sentinel)    │  argv[3] = NULL
│  ptr → "bar"       (argv[2])    │  argv[2]
│  ptr → "foo"       (argv[1])    │  argv[1]
│  ptr → "hello.elf" (argv[0])    │  argv[0]
├──────────────────────────────────┤
│  argv              (ptr to ↑)   │  pointer to argv[0]
│  argc = 3                        │  ← ESP points here after iret
└──────────────────────────────────┘
Low address (stack top, ESP)
```

When `_start` begins executing, ESP points to `argc`. The function reads:
- `[ESP]`   → argc (integer)
- `[ESP+4]` → argv (pointer to pointer array)

### Why the Stack?

- **No registers needed** — works regardless of calling convention
- **Variable size** — the kernel can push any number of arguments
- **Standard** — matches what GCC/libc `_start` → `main(argc, argv)` expects
- **Private per-process** — each task's stack page is in its own address space

### The ApPa Approach

ApPa's user stack is a single 4 KB page mapped at virtual address
`USER_STACK_VIRT` (0xBFFFF000), with ESP starting at `USER_STACK_TOP`
(0xC0000000). The stack grows downward.

Since the stack page is physically allocated by the ELF loader and
identity-mapped in the kernel, we can write directly to the physical page
before the task starts. The user task will see the data at the
corresponding virtual addresses.

**Key insight**: We write to the physical page using kernel pointers,
but compute all embedded pointers using the virtual addresses the user
process will see. This is the same technique used for the iret frame.

---

## Theory — crt0 (C Runtime Zero)

### What Is crt0?

On every Unix/Linux system, user programs don't actually start at `main()`.
They start at `_start`, a small assembly or C stub provided by the C runtime
library. This stub is called **crt0** (C Runtime Zero — the very first thing
that runs).

crt0's job is:
1. **Read argc/argv** from the stack (placed there by the kernel)
2. **Call main(argc, argv)** — the programmer's entry point
3. **Call exit()** with main's return value — ensures clean process teardown
   even if the programmer forgets to call `exit()` explicitly

```
Kernel                    crt0                      User code
───────                   ────                      ─────────
iret → _start()    →    read [esp], [esp+4]   →   main(argc, argv)
                         ret = main(argc,argv)      return 0;
                         sys_exit(ret)          ←   ←──────────
```

Without crt0, every user program must hand-code `_start` with inline
assembly to read the stack, and must explicitly call `sys_exit()` at the
end. crt0 eliminates this boilerplate.

### ApPa's crt0

A minimal `user/crt0.c` that:
- Is always linked as the **first object** in every user ELF
- Defines `_start` (matches `ENTRY(_start)` in `link.ld`)
- Reads argc/argv via inline ASM
- Calls `extern int main(int argc, char **argv)`
- Calls `sys_exit(ret)` after main returns
- Contains an infinite `hlt` loop as a safety net

```c
/* user/crt0.c — C Runtime Zero for ApPa user programs */
#include "ulibc.h"

extern int main(int argc, char **argv);

void _start(void) {
    int argc;
    char **argv;

    __asm__ volatile("mov (%%esp), %0" : "=r"(argc));
    __asm__ volatile("mov 4(%%esp), %0" : "=r"(argv));

    int ret = main(argc, argv);

    sys_exit(ret);
    for (;;) __asm__ volatile("hlt");
}
```

---

## Theory — User-Space Libc

### The Problem

Currently `libc/syscall.c`, `libc/string.c`, and `libc/stdio.c` are
compiled into the **kernel** binary. User ELF programs can't call those
functions — they're in the kernel's address space, not linked into the
user binary.

User programs in `user/hello.c` currently duplicate everything inline:
`syscall0()`, `syscall2()`, `strlen_u()`, `itoa_u()`, `write_str()`.
This doesn't scale. Each new program would have to re-implement them.

### The Solution

Create a small **user-space library** (`user/ulibc.c` + `user/ulibc.h`)
that provides:

| Category | Functions |
|----------|-----------|
| **Syscall wrappers** | `sys_exit()`, `sys_write()`, `sys_read()`, `sys_getpid()`, `sys_yield()`, `sys_sleep()` |
| **String utilities** | `strlen()`, `strcmp()`, `strcpy()`, `memcpy()`, `memset()` |
| **Output helpers** | `puts()` (write string + newline), `putchar()`, `itoa()` |
| **Types** | `uint32_t`, `int32_t`, `NULL`, `size_t` |

These are compiled as regular freestanding C — no kernel headers, no magic.
They use `INT 0x80` inline assembly for syscalls, same ABI as the kernel
expects.

### Build Pipeline

```
Before (Phase 16):
  hello.c  →  hello.o  →  hello.elf

After (Phase 17):
  crt0.c   →  crt0.o  ─┐
  ulibc.c  →  ulibc.o ─┼→  ld -T link.ld  →  hello.elf
  hello.c  →  hello.o ─┘
```

The makefile links `crt0.o` first (so `_start` is at the entry), then
the user program, then `ulibc.o`. Every user program gets crt0 + ulibc
automatically.

### Directory Layout After Phase 17

```
user/
├── crt0.c          # C Runtime Zero (_start → main bridge)
├── ulibc.c         # User-space libc implementation
├── ulibc.h         # User-space libc header
├── link.ld         # Linker script (unchanged)
├── hello.c         # Updated: now uses main() + ulibc.h
└── hello_elf.h     # Auto-generated (xxd embed)
```

---

### API Changes

#### `elf.h` — New function signatures

```c
/**
 * elf_exec — Load ELF from filesystem with arguments
 * @filename:  SimpleFS file name
 * @task_name: Human-readable name for the task
 * @argv:      NULL-terminated array of argument strings (can be NULL)
 * @argc:      Number of arguments
 */
task_t* elf_exec(const char *filename, const char *task_name,
                 const char **argv, int argc);

/**
 * elf_exec_mem — Load ELF from memory buffer with arguments
 */
task_t* elf_exec_mem(const void *buf, uint32_t size,
                     const char *task_name,
                     const char **argv, int argc);
```

When `argv` is NULL or `argc` is 0, behavior is identical to current
(ESP = USER_STACK_TOP, no arguments on stack).

#### `elf.c` — Internal changes

```c
/**
 * setup_user_stack_args — Write argv/argc onto the user stack page
 * @ustack_phys: Physical address of the user stack page
 * @argv:        NULL-terminated argument string array
 * @argc:        Number of arguments
 *
 * Returns: The virtual ESP value the user task should start with,
 *          or USER_STACK_TOP if argc == 0 (no arguments).
 */
static uint32_t setup_user_stack_args(uint32_t ustack_phys,
                                      const char **argv, int argc);
```

### Stack Layout Algorithm

Given a physical stack page at `ustack_phys` and virtual base
`USER_STACK_VIRT` (0xBFFFF000):

```
Page size:   4096 bytes
Virtual range: USER_STACK_VIRT .. USER_STACK_TOP (0xBFFFF000 .. 0xC0000000)
```

**Step 1 — Copy string data** (top of page, growing down):

```c
// Start writing strings at the top of the physical page
uint8_t *page_top = (uint8_t *)(ustack_phys + PAGE_SIZE);
uint8_t *str_ptr  = page_top;

for (int i = argc - 1; i >= 0; i--) {
    uint32_t len = strlen(argv[i]) + 1;  // include NUL
    str_ptr -= len;
    memcpy(str_ptr, argv[i], len);
    // Record the VIRTUAL address for this string
    virt_addrs[i] = USER_STACK_TOP - (page_top - str_ptr);
}
```

**Step 2 — Align to 4 bytes** (x86 stack must be 4-byte aligned):

```c
str_ptr = (uint8_t *)((uint32_t)str_ptr & ~0x3);
```

**Step 3 — Write pointer array + argc** (below the strings):

```c
uint32_t *slot = (uint32_t *)str_ptr;

*(--slot) = 0;              // NULL sentinel (argv[argc])
for (int i = argc - 1; i >= 0; i--)
    *(--slot) = virt_addrs[i];  // argv[i] pointer

uint32_t argv_virt = /* virtual address of slot (argv[0]) */;
*(--slot) = argv_virt;      // argv pointer
*(--slot) = (uint32_t)argc; // argc

// Return the virtual ESP = USER_STACK_TOP - (page_top - (uint8_t*)slot)
```

**Step 4 — Return the virtual ESP**, which `task_create_user_mapped` will
place in the iret frame instead of `USER_STACK_TOP`.

### Changes to `task_create_user_mapped`

The function signature gains a `user_esp` parameter:

```c
task_t* task_create_user_mapped(uint32_t entry_vaddr, const char *name,
                                 void *dir, uint32_t dir_phys,
                                 uint32_t user_esp);
```

The iret frame uses `user_esp` instead of the fixed `USER_STACK_TOP`:

```c
*(--sp) = user_esp;   /* ESP for iret — may be below USER_STACK_TOP */
```

When called without arguments, pass `USER_STACK_TOP` as before.

### Shell Parsing

The `cmd_exec` handler in `shell.c` splits the input into tokens:

```c
static void cmd_exec(const char* args) {
    // args = "hello.elf foo bar"
    // Split into: argv[0]="hello.elf", argv[1]="foo", argv[2]="bar"

    const char *argv[MAX_EXEC_ARGS + 1];  // +1 for NULL sentinel
    int argc = 0;

    // Tokenize by spaces (in-place or with a local copy)
    // ...

    argv[argc] = NULL;  // sentinel

    task_t *t = elf_exec(argv[0], argv[0], argv, argc);
    // ...
}
```

### User Program Entry

User programs receive arguments via the stack. The `_start` function reads:

```c
void _start(void) {
    int argc;
    char **argv;

    // GCC pushes nothing before _start — ESP points to argc
    __asm__ volatile(
        "mov (%%esp), %0\n"
        "lea 4(%%esp), %1\n"
        : "=r"(argc), "=r"(argv)
    );

    // Now argc and argv are usable
    // ...
}
```

Or, more cleanly, define `_start` to call a `main(int argc, char **argv)`:

```c
// In a small crt0-style stub:
void _start(void) {
    int argc    = *(int *)__builtin_frame_address(0);  // simplified
    char **argv = *(char ***)(/* esp+4 */);
    int ret = main(argc, argv);
    syscall0(SYS_EXIT);
}

int main(int argc, char **argv) {
    // Standard C entry
}
```

For ApPa's freestanding programs, the simplest approach is inline ASM
in `_start` that reads `[esp]` and `[esp+4]`, then passes them to
`main()`.

---

## Implementation Plan

### Step 1 — `setup_user_stack_args()` in `elf.c`

New static function. Takes physical stack page address, argv, argc.
Writes strings + pointer array + argc onto the page. Returns virtual ESP.

**Bounds check**: Total argument data must fit in one stack page minus
256 bytes (reserve space for the user function's own stack frames).
If it doesn't fit, log an error and return `USER_STACK_TOP` (no args).

### Step 2 — Update `elf_load_internal()`

Pass argv/argc through. After allocating and mapping the user stack page,
call `setup_user_stack_args()` to get the adjusted ESP. Pass that ESP
to `task_create_user_mapped()`.

### Step 3 — Update `task_create_user_mapped()`

Add `uint32_t user_esp` parameter. Use it in the iret frame instead of
the fixed `USER_STACK_TOP`.

### Step 4 — Update `elf_exec()` and `elf_exec_mem()`

Add argv/argc parameters. Thread them through to `elf_load_internal()`.

### Step 5 — Update `cmd_exec()` in `shell.c`

Tokenize the argument string into an argv array. Pass to `elf_exec()`.

### Step 6 — Create `user/ulibc.h` and `user/ulibc.c`

**`ulibc.h`** — Public header for user programs:

```c
#ifndef ULIBC_H
#define ULIBC_H

/* ── Types ────────────────────────────────────── */
typedef unsigned int   uint32_t;
typedef int            int32_t;
typedef unsigned short uint16_t;
typedef unsigned char  uint8_t;
typedef unsigned int   size_t;
#define NULL ((void *)0)

/* ── Syscall wrappers ─────────────────────────── */
void  sys_exit(int code);
int   sys_write(const char *buf, int len);
int   sys_read(char *buf, int max);
int   sys_getpid(void);
void  sys_yield(void);
void  sys_sleep(int ms);

/* ── String functions ─────────────────────────── */
int    strlen(const char *s);
int    strcmp(const char *a, const char *b);
char  *strcpy(char *dst, const char *src);
void  *memcpy(void *dst, const void *src, size_t n);
void  *memset(void *dst, int val, size_t n);

/* ── Output helpers ───────────────────────────── */
void  puts(const char *s);
void  putchar(char c);
void  print_int(int val);

#endif
```

**`ulibc.c`** — Implements all of the above using `INT 0x80` for syscalls
and pure C for string operations. No kernel headers included.

### Step 7 — Create `user/crt0.c`

The C Runtime Zero stub as described in the theory section. Reads
argc/argv from the stack, calls `main()`, calls `sys_exit()`.

### Step 8 — Update `user/hello.c` to use `main()`

Rewrite hello.c to use the standard entry point:

```c
#include "ulibc.h"

int main(int argc, char **argv) {
    puts("Hello from ELF!");

    puts("  argc = ");
    print_int(argc);
    putchar('\n');

    for (int i = 0; i < argc; i++) {
        puts("  argv[");
        print_int(i);
        puts("] = \"");
        sys_write(argv[i], strlen(argv[i]));
        puts("\"");
        putchar('\n');
    }

    return 0;   /* crt0 calls sys_exit(0) */
}
```

### Step 9 — Update makefile build pipeline

- Compile `user/crt0.c` → `user/crt0.o`
- Compile `user/ulibc.c` → `user/ulibc.o`
- For each user program `user/X.c`:
  - Compile `user/X.c` → `user/X.o`
  - Link: `ld -T link.ld crt0.o X.o ulibc.o → X.elf`
- `crt0.o` and `ulibc.o` are built once and reused for all user programs
- Update the `clean` target to remove `crt0.o` and `ulibc.o`

### Step 10 — Update tests

Add test cases in `test_elf.c`:
- `elf_exec_mem()` with argc=0 (backward compatible, no args)
- `elf_exec_mem()` with argc=3 (verify task spawns and prints args)
- Verify `main()` return value flows through to `sys_exit()` (if
  the kernel tracks exit codes in the future)

### Step 11 — Update callers

Any existing calls to the old `elf_exec()` / `elf_exec_mem()` /
`task_create_user_mapped()` signatures must be updated to pass the
new parameters (NULL/0 for no arguments, USER_STACK_TOP for ESP).

---

## Files Changed

| File | Change |
|------|--------|
| `kernel/exec/elf.h` | New function signatures (argv/argc params) |
| `kernel/exec/elf.c` | `setup_user_stack_args()`, updated `elf_load_internal()`, `elf_exec()`, `elf_exec_mem()` |
| `kernel/task/task.h` | `task_create_user_mapped()` gains `user_esp` param |
| `kernel/task/task.c` | Use `user_esp` in iret frame instead of fixed `USER_STACK_TOP` |
| `shell/shell.c` | `cmd_exec()` tokenizes args, passes argv/argc |
| `user/crt0.c` | **NEW** — C Runtime Zero (_start → main bridge) |
| `user/ulibc.h` | **NEW** — User-space libc header |
| `user/ulibc.c` | **NEW** — User-space libc implementation (syscalls, strings, output) |
| `user/hello.c` | Rewritten: uses `int main(int argc, char **argv)` + `ulibc.h` |
| `makefile` | Updated user build pipeline: crt0.o + ulibc.o auto-linked |
| `tests/test_elf.c` | New test cases for argv passing |

---

## Constraints & Edge Cases

- **Max argument size**: Total string data + pointers must fit in
  `PAGE_SIZE - 256` bytes (~3840 bytes). This allows ~100 short args.
- **Empty args**: `exec hello.elf` with no extra args → argc=1,
  argv={"hello.elf"} (the program name is always argv[0]).
- **Alignment**: All stack pointers must be 4-byte aligned for x86.
- **NUL termination**: Every string is NUL-terminated. The argv array
  is NULL-terminated after the last entry.
- **Backward compatibility**: Existing `elftest` and other callers
  pass NULL/0 for argv/argc → behavior unchanged (ESP = USER_STACK_TOP).
- **Virtual vs physical**: String pointers in the argv array are
  **virtual addresses** (0xBFFFxxxx range). The kernel writes them
  using physical addresses but computes them relative to `USER_STACK_VIRT`.

---

## Verification

After implementation, running:

```
> exec hello.elf foo bar baz
```

Should produce:

```
Hello from ELF!
  argc = 4
  argv[0] = "hello.elf"
  argv[1] = "foo"
  argv[2] = "bar"
  argv[3] = "baz"
```

Running with no arguments:

```
> exec hello.elf
```

Should produce:

```
Hello from ELF!
  argc = 1
  argv[0] = "hello.elf"
```

And `elftest` should still pass all 14 existing tests plus the new
argument-passing tests.

### What "Works" Means

- `hello.c` uses `int main(int argc, char **argv)` — no inline ASM
- `crt0.c` handles `_start` → `main()` → `sys_exit()` transparently
- User programs only `#include "ulibc.h"` for syscalls, strings, output
- The makefile auto-links `crt0.o` + `ulibc.o` into every user ELF
- Adding a new user program is just: create `user/foo.c` with a `main()`,
  run `make build`, then `exec foo.elf` in the shell
