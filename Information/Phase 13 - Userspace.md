# Phase 13: Userspace — Ring 3 Processes & System Calls

## Overview
Move execution out of kernel mode (Ring 0) into user mode (Ring 3), establishing a privilege boundary between the kernel and application code. This phase introduces system calls via `INT 0x80`, a syscall dispatch table, a user-mode task creation path that uses `iret` to drop privilege, and per-process user stacks. The result is the ability to launch isolated user-mode programs that request kernel services through a controlled gate.

**Goal:** Run user-mode tasks at Ring 3 with their own stacks, serviced by kernel syscalls dispatched through `INT 0x80`.

**Why we need this:**
- **Protection** — User code running at Ring 0 can corrupt kernel data, reprogram hardware, or disable interrupts. Ring 3 restricts dangerous instructions (`cli`, `hlt`, `lgdt`, `mov crN`, port I/O) to the kernel.
- **Fault isolation** — A buggy user program triggers a General Protection Fault (#GP) or Page Fault (#PF) instead of silently corrupting the system. The kernel's ISR catches these and can terminate only the offending task.
- **Controlled kernel access** — System calls provide a narrow, auditable interface. User code cannot call arbitrary kernel functions; it must use registered syscall numbers.
- **Foundation for processes** — Combined with per-task page directories (future), Ring 3 isolation turns tasks into true processes with separate address spaces.
- **OS maturity** — Every real OS (Linux, Windows, xv6) enforces user/kernel separation. This phase brings ApPa in line with standard OS design.

---

## Theory

### x86 Protection Rings

The x86 CPU has 4 privilege levels (rings 0–3), enforced by the hardware:

```
    ┌────────────────────────────────────────┐
    │              Ring 0 (Kernel)            │  Full hardware access
    │  ┌──────────────────────────────────┐  │
    │  │         Ring 1 (unused)          │  │
    │  │  ┌────────────────────────────┐  │  │
    │  │  │      Ring 2 (unused)       │  │  │
    │  │  │  ┌──────────────────────┐  │  │  │
    │  │  │  │   Ring 3 (User)      │  │  │  │  Restricted — no I/O,
    │  │  │  │   applications run   │  │  │  │  no cli/sti, no hlt
    │  │  │  └──────────────────────┘  │  │  │
    │  │  └────────────────────────────┘  │  │
    │  └──────────────────────────────────┘  │
    └────────────────────────────────────────┘
```

The **Current Privilege Level (CPL)** is stored in the low 2 bits of `CS`. Ring 0 code uses `CS = 0x08` (GDT entry 1, RPL=0). Ring 3 code uses `CS = 0x1B` (GDT entry 3, RPL=3). The CPU checks CPL against segment DPL on every instruction fetch, memory access, and privileged operation.

### How Privilege Transitions Work

#### Ring 3 → Ring 0 (Interrupt / Syscall)

When a user-mode program executes `INT 0x80`, the CPU:
1. Reads `SS0:ESP0` from the current **TSS** (the kernel stack)
2. Pushes the user's `SS`, `ESP`, `EFLAGS`, `CS`, `EIP` onto the **kernel stack**
3. Loads `CS:EIP` from the **IDT** entry for vector 0x80
4. CPL changes to 0 (kernel mode)

```
User stack (Ring 3)           Kernel stack (Ring 0)
┌────────────────┐            ┌────────────────────────┐
│  user code     │            │  SS3      (user SS)    │ ← pushed by CPU
│  locals, args  │            │  ESP3     (user ESP)   │
│  ...           │            │  EFLAGS                │
└────────────────┘            │  CS3      (user CS)    │
                              │  EIP3     (user EIP)   │
                              │  err code (0 for INT)  │ ← pushed by stub
                              │  int_no   (0x80)       │
                              │  pusha registers       │ ← pushed by stub
                              │  ds (saved)            │
                              └────────────────────────┘
                                        ↓
                              syscall_handler() runs here
```

This is exactly why Phase 12 set up the **TSS** with `esp0` — the CPU needs it to find the kernel stack during this privilege transition.

#### Ring 0 → Ring 3 (iret)

To enter user mode, the kernel pushes a fake interrupt frame onto the kernel stack and executes `iret`:

```asm
push USER_DATA_SEG    ; SS   (0x23)
push user_esp         ; ESP  (top of user stack)
pushf                 ; EFLAGS (with IF=1)
push USER_CODE_SEG    ; CS   (0x1B)
push user_eip         ; EIP  (entry point)
iret                  ; pops all 5 and jumps to Ring 3
```

The CPU sees that `CS` has RPL=3, sets CPL=3, and begins executing at `user_eip` in user mode.

### System Call Mechanism

A system call is a software interrupt (`INT 0x80`) that provides a controlled entry point from user mode to kernel mode. The calling convention:

```
Register    Purpose
────────    ──────────────────────────
EAX         Syscall number (index into dispatch table)
EBX         Argument 1
ECX         Argument 2
EDX         Argument 3
ESI         Argument 4
EDI         Argument 5
EAX         Return value (set by kernel before iret)
```

This matches the Linux i386 syscall ABI, making it familiar and well-documented.

### User Stack vs Kernel Stack

Each user-mode task needs **two stacks**:

```
┌──────────────────────────────────────────────────┐
│                  Task Memory                      │
│                                                   │
│  ┌─────────────┐       ┌─────────────────────┐   │
│  │ User Stack  │       │   Kernel Stack      │   │
│  │ (Ring 3)    │       │   (Ring 0)          │   │
│  │             │       │                     │   │
│  │ Local vars  │       │ Saved user context  │   │
│  │ Return addrs│       │ Syscall frames      │   │
│  │ Syscall args│       │ ISR scratch space   │   │
│  │             │       │                     │   │
│  │ grows ↓     │       │ grows ↓             │   │
│  └─────────────┘       └─────────────────────┘   │
│                                                   │
│  Accessible by user    Only accessible by kernel  │
└──────────────────────────────────────────────────┘
```

- **User stack** — Used by the program for function calls, local variables. Mapped in user-accessible pages (PTE user bit = 1). Typically 4-8 KB.
- **Kernel stack** — Used by the CPU when an interrupt or syscall occurs. Contains the saved user registers. Mapped in kernel-only pages. Already allocated by Phase 12's `task_create()`.

### Page Table User Bit

The x86 page table entry has a **User/Supervisor** bit (bit 2):

```
PTE bits:
  Bit 0: Present
  Bit 1: Read/Write
  Bit 2: User/Supervisor  ← 0 = kernel only, 1 = user accessible
  ...
```

When CPL=3, the CPU only allows access to pages where the U/S bit is set in both the page directory entry and page table entry. Kernel pages (U/S=0) are invisible to user code — any access triggers a page fault.

---

## Implementation Plan

### Step 1: Syscall Infrastructure

**Files:** `kernel/arch/syscall.h`, `kernel/arch/syscall.c`, `kernel/arch/syscall_stub.asm`

1. Register IDT entry 0x80 as a **trap gate** (not interrupt gate) with DPL=3, so user code can invoke it
2. Write an assembly stub (`syscall_stub.asm`) that:
   - Pushes interrupt number (0x80) and a dummy error code
   - Saves all registers (pusha + ds)
   - Loads kernel data segment (0x10)
   - Calls the C `syscall_dispatcher(registers_t *regs)`
   - Restores registers and `iret`s back to user mode
3. The C dispatcher reads `regs->eax` as the syscall number, indexes into a function pointer table, and calls the handler with args from EBX/ECX/EDX/ESI/EDI
4. Return value placed in `regs->eax`

```c
// Syscall numbers
#define SYS_EXIT    0   // Terminate current task
#define SYS_WRITE   1   // Write to screen: write(buf, len)
#define SYS_READ    2   // Read from keyboard buffer: read(buf, max)
#define SYS_YIELD   3   // Voluntarily yield CPU
#define SYS_GETPID  4   // Get current task ID
#define SYS_SLEEP   5   // Sleep for N milliseconds

// Dispatch table
typedef int (*syscall_fn_t)(registers_t *);
syscall_fn_t syscall_table[MAX_SYSCALLS];
```

### Step 2: User-Mode Task Creation

**Files:** `kernel/task/task.c` (modified), `kernel/task/umode.asm`

Extend `task_create()` or add `task_create_user()` that:

1. Allocates a **user stack** page from PMM (4 KB, mapped with U/S=1)
2. Allocates a **kernel stack** page from PMM (4 KB, mapped with U/S=0) — already done in Phase 12
3. Builds a fake **iret frame** on the kernel stack instead of the Phase 12 fake context frame:

```
Kernel stack layout for new user task:

  [high]  USER_DATA_SEG  (0x23)     ← SS for iret
          user_stack_top             ← ESP for iret
          EFLAGS | 0x200            ← IF=1 (interrupts enabled)
          USER_CODE_SEG  (0x1B)     ← CS for iret
          user_entry_point          ← EIP for iret
  [low]   ... (task_switch frame as before)
```

4. The first context switch lands in a trampoline (`enter_usermode`) that executes `iret` to drop to Ring 3

### Step 3: User-Mode Entry Trampoline

**File:** `kernel/task/umode.asm`

```nasm
; enter_usermode - Pop iret frame and jump to Ring 3
; Called after task_switch restores the new task's kernel stack
enter_usermode:
    ; Stack has: EIP, CS, EFLAGS, ESP, SS (pushed by task_create_user)
    iret
```

### Step 4: User-Side Syscall Library

**File:** `libc/syscall.h`, `libc/syscall.c`

Provide thin wrappers that user programs call:

```c
static inline int syscall(int num, int a1, int a2, int a3) {
    int ret;
    __asm__ volatile(
        "int $0x80"
        : "=a"(ret)
        : "a"(num), "b"(a1), "c"(a2), "d"(a3)
        : "memory"
    );
    return ret;
}

// Convenience wrappers
void sys_exit(void);
int  sys_write(const char *buf, int len);
int  sys_read(char *buf, int max);
void sys_yield(void);
int  sys_getpid(void);
void sys_sleep(int ms);
```

### Step 5: Kernel-Side Syscall Handlers

**File:** `kernel/arch/syscall.c`

Implement each syscall handler:

| Syscall | Handler | Description |
|---------|---------|-------------|
| `SYS_EXIT` | `sys_exit_handler` | Calls `task_exit()` |
| `SYS_WRITE` | `sys_write_handler` | Validates user buffer pointer, copies to kernel, calls `kprint()` |
| `SYS_READ` | `sys_read_handler` | Reads from keyboard ring buffer (future: blocking) |
| `SYS_YIELD` | `sys_yield_handler` | Calls `sched_yield()` |
| `SYS_GETPID` | `sys_getpid_handler` | Returns `task_get_current()->id` |
| `SYS_SLEEP` | `sys_sleep_handler` | Marks task BLOCKED, sets wakeup tick, yields |

**Critical: pointer validation.** Every syscall that receives a user pointer must verify it falls within user-accessible memory before dereferencing:

```c
static int validate_user_ptr(const void *ptr, uint32_t len) {
    uint32_t start = (uint32_t)ptr;
    uint32_t end   = start + len;
    // Check: within user memory range, doesn't wrap, pages are user-mapped
    if (start < USER_SPACE_START || end > USER_SPACE_END || end < start)
        return -1;
    return 0;
}
```

### Step 6: Page Table Modifications

**File:** `kernel/mem/paging.c` (modified)

Add support for user-accessible pages:

1. `paging_map_user(virt, phys, flags)` — maps a page with the U/S bit set
2. User stack pages mapped at a fixed virtual address range (e.g. `0xBFFFF000` downward)
3. Kernel pages remain supervisor-only (U/S=0)

With identity mapping (Phase 10), user code initially shares the same physical-to-virtual mapping. Per-process page directories come in a future phase.

### Step 7: Test User-Mode Task

Create a simple user program that:
1. Calls `sys_write("Hello from Ring 3!\n", 19)` via `INT 0x80`
2. Calls `sys_getpid()` and prints its task ID
3. Calls `sys_exit()`

Verify:
- The program executes at CPL=3 (check CS in a breakpoint)
- Attempting `cli` or `hlt` in user mode triggers #GP
- Syscalls work and return correct values
- User task can be preempted by the timer
- Task exit works and resources are reclaimed

---

## Memory Layout Impact

```
Current (identity mapped, Phase 10+12):

0x00000000 ─┬─────────────────────────────────
             │ Kernel code, data, BSS          (Ring 0 only)
0x00001000  ─┤ Loaded by bootloader
             │ ...
0x0009FC00  ─┤ Boot stack top
             │
0x000A0000  ─┤ VGA memory                      (Ring 0 I/O)
0x00100000  ─┤ Kernel heap (kmalloc)            (Ring 0 only)
0x00200000  ─┤ PMM bitmap + page pool           (Ring 0 only)
             │ ...
0x00F00000  ─┤ Reserved
0x01000000  ─┤ ── New: User space begins ──     (Ring 3 accessible)
             │ User code + user stacks
             │ Mapped with PTE U/S=1
             │ ...
```

Each user task adds:
- 1 user stack page (4 KB, U/S=1)
- 1 kernel stack page (4 KB, U/S=0) — already from Phase 12
- 1 TCB slot from the static pool

---

## Risks and Considerations

### Stack Pointer Validation
The CPU loads SS:ESP from the iret frame when returning to Ring 3. If the user stack pointer is invalid, the CPU double-faults. The kernel must ensure the user stack is properly mapped before the first iret.

### Syscall Number Bounds Check
The dispatcher must validate `regs->eax < MAX_SYSCALLS` before indexing the table. An out-of-bounds index could jump to an arbitrary address.

### Re-entrancy
A syscall handler runs with interrupts enabled (trap gate, not interrupt gate). The timer can preempt the handler mid-execution. Handlers that touch shared state (e.g. screen driver) may need `cli`/`sti` guards or proper locking (future phase).

### GDT Segments Already Ready
Phase 12 already installed user code (`0x1B`) and user data (`0x23`) segments in the GDT. No GDT changes are needed.

### TSS Already Functional
Phase 12's TSS management (updating `esp0` on every context switch) means the CPU will correctly find each task's kernel stack during the Ring 3 → Ring 0 transition. No TSS changes needed.

---

## What Changes and Why

### Modified Files

| File | Change | Reason |
|------|--------|--------|
| `kernel/arch/idt.c` | Register vector 0x80 as DPL=3 trap gate | Allow `INT 0x80` from Ring 3 |
| `kernel/mem/paging.c` | Add `paging_map_user()` | Map user stack pages with U/S=1 |
| `kernel/task/task.c` | Add `task_create_user()` | Build iret frame + user stack |
| `makefile` | Compile new files | New .c and .asm sources |

### New Files

| File | Purpose |
|------|---------|
| `kernel/arch/syscall.h` | Syscall numbers, dispatcher prototype |
| `kernel/arch/syscall.c` | Dispatch table, syscall handlers, pointer validation |
| `kernel/arch/syscall_stub.asm` | INT 0x80 assembly entry point |
| `kernel/task/umode.asm` | `enter_usermode` trampoline (iret to Ring 3) |
| `libc/syscall.h` | User-side `INT 0x80` inline wrappers |
| `libc/syscall.c` | Convenience functions (sys_write, sys_exit, ...) |
| `tests/test_userspace.c/h` | Ring 3 task tests |

---

## API Reference (Planned)

### Syscall Interface (`INT 0x80`)

| # | Name | Args | Returns | Description |
|---|------|------|---------|-------------|
| 0 | `SYS_EXIT` | — | — | Terminate current task |
| 1 | `SYS_WRITE` | EBX=buf, ECX=len | bytes written | Write to screen |
| 2 | `SYS_READ` | EBX=buf, ECX=max | bytes read | Read keyboard input |
| 3 | `SYS_YIELD` | — | 0 | Yield CPU |
| 4 | `SYS_GETPID` | — | task ID | Get current task ID |
| 5 | `SYS_SLEEP` | EBX=ms | 0 | Sleep for milliseconds |

### Kernel API

| Function | Description |
|----------|-------------|
| `syscall_init()` | Register IDT 0x80, populate dispatch table |
| `task_create_user(entry, name)` | Create a Ring 3 task with user + kernel stacks |
| `paging_map_user(virt, phys, rw)` | Map a page with U/S=1 |
| `validate_user_ptr(ptr, len)` | Check user pointer is in valid user memory |

---

## Success Criteria

1. A user-mode task executes with `CS & 3 == 3` (CPL=3 confirmed via GDB)
2. `INT 0x80` with `EAX=SYS_WRITE` prints to screen from Ring 3
3. Executing `cli` in user mode triggers #GP exception
4. The kernel catches the #GP and terminates only the faulting task
5. Preemptive scheduling still works for user tasks
6. `sys_exit()` properly cleans up and reclaims resources
7. Multiple user tasks run concurrently alongside the kernel shell

---

## Implementation Status: COMPLETE

All success criteria have been verified. Phase 13 was implemented and tested successfully.

### Files Created

| File | Purpose |
|------|---------|
| `kernel/arch/syscall.h` | Syscall numbers (0-5), dispatch table type, user memory bounds, API prototypes |
| `kernel/arch/syscall.c` | Dispatch table, 6 syscall handlers, pointer validation, GPF handler for Ring 3 faults |
| `kernel/arch/syscall_stub.asm` | INT 0x80 assembly entry — saves registers into `registers_t`, loads kernel segments, calls C dispatcher, `iret` back |
| `kernel/task/umode.asm` | `enter_usermode` trampoline — single `iret` that drops to Ring 3 |
| `libc/syscall.h` | User-side inline `INT 0x80` wrappers (`syscall0`-`syscall3`) and convenience functions |
| `libc/syscall.c` | `sys_exit()`, `sys_write()`, `sys_read()`, `sys_yield()`, `sys_getpid()`, `sys_sleep()` |
| `tests/test_userspace.h` | Test header |
| `tests/test_userspace.c` | 3 tests: SYS_WRITE+GETPID+EXIT, SYS_YIELD, GPF from Ring 3 |

### Files Modified

| File | Change |
|------|--------|
| `kernel/arch/idt.h` | Exposed `idt_set_gate()` declaration |
| `kernel/mem/paging.h` | Added `paging_map_user()`, `paging_enable_user_access()` |
| `kernel/mem/paging.c` | `paging_map_user()`, `paging_enable_user_access()`, PDE PAGE_USER propagation |
| `kernel/task/task.h` | Added `user_stack`, `user_stack_top`, `is_user` to `task_t`; declared `task_create_user()` |
| `kernel/task/task.c` | `task_create_user()` (allocates user+kernel stacks, builds iret frame), `task_reap()` (frees user stacks) |
| `kernel/sys/kernel_main.c` | Added `syscall_init()` call after scheduler init |
| `shell/shell.c` | Added `usertest` shell command |
| `tests/tests.h` | Included `test_userspace.h` |

### Test Results

All tests pass. Run via `usertest` shell command:

```
--- Test: Userspace (Ring 3) ---
  [1] Creating user task 'hello'...
  [OK] task_create_user returned TCB id=1
Hello from Ring 3!
  [OK] SYS_WRITE completed
  [OK] SYS_GETPID returned 1
  [OK] SYS_EXIT reached
  [2] Creating user task 'yield'...
  [OK] SYS_YIELD invoked 4 times
  [3] Creating user task 'gpf' (will trigger #GP)...
[GPF] User task 'u:gpf' caused General Protection Fault at EIP=0x00006996 err=0x00000000 -- killed.
  [OK] GPF killed user task (system survived)
--- Userspace tests complete ---
```

### Known Limitations (Addressed in Phase 15)

- **No memory isolation** — `paging_enable_user_access()` marks the entire 0-16 MB identity map as user-accessible. Any Ring 3 task can read/write kernel data. Phase 15 introduces per-process page directories.
- **User code in kernel binary** — User task functions are compiled into the kernel. A future ELF loader phase will load user programs from disk.
- **SYS_READ stub** — Returns 0 (no data). Full keyboard ring-buffer integration is future work.
- **SYS_SLEEP busy-yields** — Uses a yield loop instead of proper BLOCKED state + wakeup. Proper sleep queues are future work.
