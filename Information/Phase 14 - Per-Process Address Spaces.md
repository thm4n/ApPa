# Phase 14: Per-Process Address Spaces

## Overview
Give each user-mode task its own page directory so it runs in an isolated virtual address space. A faulting or malicious user process can no longer read, write, or corrupt another process's memory — or the kernel's data structures. This phase clones the kernel's page directory for every new task, maps user code and stack into a private region, switches CR3 on every context switch, and reclaims the per-process page tables on exit.

**Goal:** Each user task gets a private page directory; kernel pages are shared (mapped identically in every directory), while user pages are per-process.

**Why we need this:**
- **True memory isolation** — Phase 13 marked *all* 0-16 MB as user-accessible (`PAGE_USER`). Any Ring 3 task can read/write kernel data, other tasks' stacks, and hardware memory. Per-process page tables eliminate this.
- **Crash containment** — A wild pointer in process A page-faults instead of silently corrupting process B. The kernel kills only the offending process.
- **Foundation for `fork`/`exec`** — POSIX process creation duplicates a page directory (`fork`) then replaces user pages with a new program (`exec`).
- **Foundation for demand paging** — Pages can be allocated lazily (on first access) since each process has its own PTE space to track what's mapped and what isn't.
- **Virtual address reuse** — Every process can use the same virtual layout (e.g. code at `0x08048000`, stack at `0xBFFFF000`) because they each have separate page directories.

---

## Theory

### Current State (Phase 13)

```
           Single Page Directory (kernel_directory)
           ┌─────────────────────────────────────────┐
           │ PDE 0: 0x000 - 0x3FF (0-4 MB)   U=1    │ ← shared
           │ PDE 1: 0x400 - 0x7FF (4-8 MB)   U=1    │ ← shared
           │ PDE 2: 0x800 - 0xBFF (8-12 MB)  U=1    │ ← shared
           │ PDE 3: 0xC00 - 0xFFF (12-16 MB) U=1    │ ← shared
           │ PDE 4-1023: not present                  │
           └─────────────────────────────────────────┘
                         ↑
                    All tasks use this (via CR3)
                    User code can touch EVERYTHING
```

**Problem:** Ring 3 code has `PAGE_USER` access to kernel text, kernel heap, PMM bitmap, other tasks' stacks — everything in the first 16 MB.

### Target State (Phase 14)

```
      Kernel Page Directory           Process A Directory          Process B Directory
      (template, never loaded)        (clone of kernel)            (clone of kernel)
      ┌───────────────────┐           ┌───────────────────┐       ┌───────────────────┐
      │ PDE 0-3: kernel   │───────────│ PDE 0-3: kernel   │       │ PDE 0-3: kernel   │
      │   U=0 (supervisor)│  shared   │   U=0 (supervisor)│       │   U=0 (supervisor)│
      │                   │  page     │                   │       │                   │
      │ PDE 4-1023:       │  tables   │ PDE 512: user code│       │ PDE 512: user code│
      │   not present     │           │   U=1 (private)   │       │   U=1 (private)   │
      └───────────────────┘           │ PDE 767: user stk │       │ PDE 767: user stk │
                                      │   U=1 (private)   │       │   U=1 (private)   │
                                      └───────────────────┘       └───────────────────┘
                                               ↑                           ↑
                                          CR3 when A runs            CR3 when B runs
```

Key properties:
- **Kernel half (PDE 0-3):** Points to the *same* physical page tables. U/S=0 so Ring 3 can't access them. Shared across all processes.
- **User half (PDE 4+):** Each process has its own page tables. U/S=1 on the relevant entries. Visible only to that process.
- **CR3 switch:** On every context switch, the scheduler loads the new task's page directory physical address into CR3.

### x86 CR3 and TLB

```
┌──────────────────────────────────────────────────────────┐
│ CR3 Register (Page Directory Base Register)              │
│                                                          │
│  bits 31-12: physical address of page directory (4 KB    │
│              aligned, so bottom 12 bits are flags/zero)  │
│  bit 4: PCD (page cache disable)                        │
│  bit 3: PWT (page write-through)                        │
│  bits 2-0: reserved (0)                                  │
└──────────────────────────────────────────────────────────┘
```

When CR3 is written, the CPU **flushes the entire TLB** (Translation Lookaside Buffer). This is the cost of a full address-space switch. On each context switch:

```c
// In sched.c switch_to():
if (next->page_directory != prev->page_directory) {
    __asm__ volatile("mov %0, %%cr3" : : "r"(next->cr3));
}
```

### Virtual Address Layout (Per-Process)

```
Virtual Address Space (4 GB total)
────────────────────────────────────────────────
0x00000000 ─┬─ Kernel space (PDE 0-3, 16 MB)
             │  Identity-mapped, U/S=0
             │  Code, heap, PMM, page tables
0x01000000 ─┤
             │  (unmapped gap)
             │
0x08048000 ─┤─ User code segment (convention)
             │  .text, .rodata, .data, .bss
             │  Mapped U/S=1, per-process
             │
0xBFFFF000 ─┤─ User stack (grows downward)
             │  4-8 KB initially, per-process
             │  Mapped U/S=1
0xC0000000 ─┤
             │  (reserved for future kernel
             │   higher-half mapping)
0xFFFFFFFF ─┘
```

For Phase 14 we keep it simple: user code stays in the kernel binary (not loaded from ELF yet), so the "user code" pages will be copies of the kernel-linked function pages re-mapped as user-accessible in each process's page directory. The user stack gets a fixed virtual address (e.g. `0xBFFFF000`) mapped to a unique physical page per process.

---

## Implementation Plan

### Step 1: Revert `paging_enable_user_access()`

**File:** `kernel/mem/paging.c`, `kernel/arch/syscall.c`

Phase 13 called `paging_enable_user_access()` to set PAGE_USER on the entire 0-16 MB identity map. This must be reverted:
- Remove the call from `syscall_init()`
- Keep the function for debugging but don't call it at boot
- The kernel's identity-mapped 0-16 MB returns to U/S=0 (supervisor only)

### Step 2: Add CR3 / Page Directory to TCB

**File:** `kernel/task/task.h`

```c
typedef struct task {
    // ... existing fields ...
    uint32_t            cr3;            // Physical address of this task's page directory
    page_directory_t   *page_dir;       // Virtual pointer to page directory
    // ...
} task_t;
```

Kernel tasks (Ring 0) continue using the kernel page directory. Only user tasks get a private clone.

### Step 3: Implement `paging_clone_directory()`

**File:** `kernel/mem/paging.c`

```c
/**
 * paging_clone_directory - Create a new page directory for a user process
 *
 * 1. Allocate a new 4 KB page for the page directory
 * 2. Copy the kernel's PDE entries 0-3 verbatim (shared page tables,
 *    supervisor-only — the same physical page table pages)
 * 3. Clear PDE entries 4-1023 (user space — to be populated per-process)
 * 4. Return the new directory and its physical address
 */
page_directory_t* paging_clone_directory(uint32_t *out_phys);
```

Since the kernel PDEs point to the same physical page tables, kernel memory is automatically visible (but not user-accessible) in every process.

### Step 4: Map User Pages Into the Clone

**File:** `kernel/mem/paging.c`

Add helpers to map pages into a *specific* page directory (not just the global `kernel_directory`):

```c
/**
 * paging_map_page_in - Map a page in a specific page directory
 * @dir:   Target page directory
 * @virt:  Virtual address
 * @phys:  Physical frame
 * @flags: PAGE_PRESENT | PAGE_WRITABLE | PAGE_USER
 */
void paging_map_page_in(page_directory_t *dir, uint32_t virt,
                        uint32_t phys, uint32_t flags);
```

For each user task we need to map:
1. **User code pages** — The physical pages containing the user entry function, mapped at the same virtual address (identity) but with PAGE_USER in the clone only
2. **User stack** — A fresh physical page mapped at a fixed virtual address (e.g. `0xBFFFF000`) with PAGE_USER | PAGE_WRITABLE

### Step 5: Update `task_create_user()`

**File:** `kernel/task/task.c`

Modify the user task creation to:
1. Call `paging_clone_directory()` to create a private page directory
2. Map the user entry function's page(s) with PAGE_USER in the clone
3. Map a user stack page at `USER_STACK_VIRT` with PAGE_USER in the clone
4. Store `cr3` and `page_dir` in the TCB
5. Build the iret frame with the virtual stack address

```c
#define USER_STACK_VIRT  0xBFFFF000  // Virtual address for user stack (top page)
#define USER_STACK_TOP   0xC0000000  // Stack grows down from here
```

### Step 6: CR3 Switch in Scheduler

**File:** `kernel/task/sched.c`

In `switch_to()`, after updating the TSS, load the new task's CR3:

```c
static void switch_to(task_t *next) {
    // ... existing TSS update ...

    // Switch address space if the new task has a different page directory
    if (next->cr3 && next->cr3 != prev->cr3) {
        __asm__ volatile("mov %0, %%cr3" : : "r"(next->cr3));
    }

    task_switch(&prev->esp, next->esp);
}
```

The kernel (idle task, kernel tasks) keeps `cr3 = 0` meaning "use the boot page directory." Only user tasks trigger a CR3 reload.

### Step 7: Free Page Directory on Task Exit

**File:** `kernel/task/task.c` (`task_reap`)

When reaping a dead user task:
1. Walk the process's page directory entries 4-1023
2. For each present PDE, walk its page table entries
3. Free every user-mapped physical page (page table entries with PAGE_USER)
4. Free the page table pages themselves
5. Free the page directory page

```c
void paging_free_directory(page_directory_t *dir, uint32_t cr3);
```

### Step 8: Update User Pointer Validation

**File:** `kernel/arch/syscall.c`

With true address spaces, `validate_user_ptr()` checks that the pointer falls in the user virtual range and that the page is actually mapped in the current process's page directory:

```c
static int validate_user_ptr(const void *ptr, uint32_t len) {
    uint32_t start = (uint32_t)ptr;
    uint32_t end   = start + len;

    if (start < USER_CODE_START || end > USER_STACK_TOP || end < start)
        return -1;

    // Walk the current task's page directory to verify pages are mapped
    // (future refinement — for now, range check is sufficient)
    return 0;
}
```

### Step 9: Test Per-Process Isolation

**File:** `tests/test_addrspace.c/h`

Tests:
1. **Separate directories** — Two user tasks have different `cr3` values
2. **Kernel invisible** — User task attempting to read kernel address (e.g. `0x00100000`) triggers page fault, only that task is killed
3. **Cross-process invisible** — Process A can't see process B's stack page
4. **Shared kernel** — Syscalls still work (kernel pages visible at Ring 0 during INT 0x80)
5. **Cleanup** — After task exit + reap, page directory and user pages are freed (PMM free count returns to pre-test value)

---

## Memory Layout Impact

```
Before (Phase 13):                    After (Phase 14):

Single page directory                 Per-process page directories
All pages USER-accessible             Kernel pages: U/S=0 (supervisor)
                                      User pages: U/S=1 (per-process)

┌─────────────┐                       ┌─────────────┐  ┌─────────────┐
│ PDE 0-3     │                       │ PDE 0-3     │  │ PDE 0-3     │
│ 0-16MB U=1  │                       │ 0-16MB U=0  │  │ 0-16MB U=0  │
│ (everyone)  │                       │ (shared PT) │  │ (shared PT) │
│             │                       │             │  │             │
│ PDE 4+      │                       │ PDE ~512    │  │ PDE ~512    │
│ not present │                       │ user code   │  │ user code   │
│             │                       │ U=1         │  │ U=1         │
│             │                       │             │  │             │
│             │                       │ PDE ~767    │  │ PDE ~767    │
│             │                       │ user stack  │  │ user stack  │
│             │                       │ U=1         │  │ U=1         │
└─────────────┘                       └─────────────┘  └─────────────┘
     ↑                                    ↑ Process A       ↑ Process B
  All tasks                            separate CR3      separate CR3
```

Per-process overhead:
- 1 page directory (4 KB)
- 1-2 page tables for user regions (4 KB each)
- 1 user stack page (4 KB)
- 1 kernel stack page (4 KB) — already from Phase 12

Total: ~16-20 KB per user process.

---

## Risks and Considerations

### TLB Flush Cost
Every CR3 write flushes the entire TLB. For a small OS with few processes and 100 Hz scheduling, this is negligible. On real hardware with many processes, techniques like PCID (Process Context Identifiers) and lazy TLB flushing would be needed.

### Kernel Page Table Sharing
The kernel PDEs (0-3) in every cloned directory point to the *same physical page tables*. If the kernel maps new kernel pages (e.g. a new page table), all cloned directories automatically see them because they share the page table pages. However, if a new PDE slot (4+) is needed for kernel use, it must be propagated to all existing process directories — a complexity we avoid by keeping the kernel within 16 MB.

### User Code Location
Phase 14 still compiles user functions into the kernel binary. The user entry point's physical pages must be mapped as PAGE_USER in the clone. Since we know the function address (it's a kernel symbol), we can compute which physical page(s) it occupies and map them. A future ELF loader phase will decouple user code from the kernel binary entirely.

### Recursive Page Directory (Optional)
Many OS tutorials map the page directory as the last PDE entry pointing to itself, enabling easy virtual-address access to page tables. We can add this optimization but it's not required for basic functionality.

### Stack Guard Page
Map the page below the user stack as not-present (no PTE). If the stack overflows, it triggers a page fault instead of silently corrupting memory. Easy to add and highly valuable.

---

## What Changes and Why

### Modified Files

| File | Change | Reason |
|------|--------|--------|
| `kernel/task/task.h` | Add `cr3`, `page_dir` to `task_t` | TCB needs to track per-task address space |
| `kernel/task/task.c` | Update `task_create_user()` and `task_reap()` | Clone directory on create, free on exit |
| `kernel/task/sched.c` | CR3 switch in `switch_to()` | Load correct address space for each task |
| `kernel/mem/paging.c` | `paging_clone_directory()`, `paging_map_page_in()`, `paging_free_directory()` | Per-process page directory management |
| `kernel/mem/paging.h` | Declare new functions | Public API |
| `kernel/arch/syscall.c` | Remove `paging_enable_user_access()` call, update `validate_user_ptr()` | Kernel pages no longer user-accessible |
| `makefile` | Compile new test file | New test source |

### New Files

| File | Purpose |
|------|---------|
| `tests/test_addrspace.c/h` | Per-process isolation tests |

---

## API Reference (Planned)

### Paging API Additions

| Function | Description |
|----------|-------------|
| `paging_clone_directory(uint32_t *out_phys)` | Clone kernel PDE 0-3 into new page directory, return virtual ptr |
| `paging_map_page_in(dir, virt, phys, flags)` | Map a page in a specific directory (not the global one) |
| `paging_free_directory(dir, cr3)` | Free all user pages, page tables, and the directory itself |

### Task API Changes

| Function | Change |
|----------|--------|
| `task_create_user()` | Now creates a private page directory with user code + stack mapped |
| `task_reap()` | Now frees the per-process page directory and all user pages |

---

## Success Criteria

1. Each user task has a unique `cr3` value (confirmed via GDB or `kprint_hex(task->cr3)`)
2. The kernel's identity-mapped 0-16 MB is **not** user-accessible (U/S=0 in PDE/PTE)
3. A user task attempting to read kernel memory (e.g. `*(volatile int*)0x100000`) triggers a page fault and is killed — system continues
4. Two user tasks cannot see each other's stack pages — writing to the other's virtual stack address faults
5. Syscalls (SYS_WRITE, SYS_GETPID, etc.) still work because the kernel pages are visible during Ring 0 execution (INT 0x80 doesn't change CR3)
6. After task exit + reap, the PMM free page count returns to its pre-creation value (no page leaks)
7. Preemptive scheduling still works; timer IRQ fires and switches between processes with different CR3 values
8. The `usertest` shell command still passes all existing tests
