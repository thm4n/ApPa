# Phase 12: Multitasking — Task Scheduler & Context Switching

## Overview
Introduce cooperative and preemptive multitasking to the kernel by building a Task State Segment (TSS), per-task kernel stacks, a Task Control Block (TCB) structure, a low-level assembly context switch, and a round-robin scheduler driven by the existing timer interrupt (IRQ0). This phase transforms the kernel from a single-threaded event loop into a system that can run multiple independent execution contexts concurrently.

**Goal:** Enable multiple kernel tasks to share the CPU via time-sliced round-robin scheduling, with each task owning its own stack and register state.

**Why we need this:**
- **Concurrency** — Run the shell, background services, and idle processing simultaneously instead of serialising everything in one `while(1)` loop
- **Responsiveness** — Long-running work no longer blocks the entire system; the timer interrupt preempts the current task and gives others a turn
- **Foundation for processes** — A task with its own address space becomes a process; this phase provides the scheduling and context-switch machinery that future Ring 3 userspace processes will reuse
- **Foundation for blocking I/O** — Tasks can be marked BLOCKED and descheduled while waiting for disk, keyboard, or timer events
- **Resource accounting** — Each task has an identity (ID, name, state), enabling future monitoring, priority schemes, and resource limits

---

## Theory

### What Is a "Task"?
In CPU terms, a task is an independent thread of execution. It consists of:

1. **A set of register values** — EAX, EBX, …, ESP, EBP, EIP, EFLAGS, segment selectors
2. **A stack** — where local variables, return addresses, and interrupt frames live
3. **A scheduling state** — ready, running, blocked, or dead

At any instant only one task actually occupies the CPU. The illusion of concurrency is created by rapidly switching between tasks — saving the registers of the outgoing task and loading the registers of the incoming task. This operation is called a **context switch**.

```
              Timer IRQ fires (every 10 ms)
                         │
  ┌──────────┐           │           ┌──────────┐
  │  Task A  │ ◄─────────┤───────── │  Task B  │
  │ (running)│  save A   │  load B  │ (ready)  │
  └──────────┘  regs     │  regs    └──────────┘
                         │
              Context Switch (≈200 ns)
```

### Cooperative vs Preemptive Multitasking

| Property | Cooperative | Preemptive |
|----------|-------------|------------|
| Who decides when to switch? | The running task calls `yield()` | The timer interrupt forces a switch |
| Can a buggy task hog the CPU? | Yes — if it never yields | No — the timer preempts it |
| Implementation complexity | Lower — no interrupt-driven switching | Higher — must save/restore state inside an ISR |
| Latency guarantees | None | Bounded by the time-slice length |

ApPa implements **both**: tasks can call `sched_yield()` cooperatively, and the timer interrupt calls `schedule()` preemptively every time-slice (100 ms by default at 100 Hz / 10 tick slices).

### The x86 Task State Segment (TSS)

The TSS is a hardware-defined 104-byte structure that the CPU consults in exactly one critical situation: **when an interrupt or exception causes a privilege-level change** (e.g. Ring 3 → Ring 0). The CPU reads the `ss0:esp0` fields from the TSS to obtain the kernel stack pointer for the interrupted task.

```
TSS (104 bytes)
┌─────────────────────────────────────────┐
│ prev_tss         (unused)               │  +0x00
│ esp0 ◄── Kernel stack pointer (Ring 0)  │  +0x04  ← We use this
│ ss0  ◄── Kernel stack segment (0x10)    │  +0x08  ← We use this
│ esp1, ss1        (Ring 1, unused)       │  +0x0C
│ esp2, ss2        (Ring 2, unused)       │  +0x14
│ cr3              (unused, manual CR3)   │  +0x1C
│ eip, eflags      (unused by us)        │  +0x20
│ eax..edi         (unused by us)        │  +0x28
│ es, cs, ss, ds, fs, gs  (unused)       │  +0x48
│ ldt              (unused)               │  +0x60
│ trap, iomap_base                        │  +0x64
└─────────────────────────────────────────┘
```

Although the TSS has fields for all general-purpose registers (which Intel designed for hardware task switching), modern operating systems — including Linux, Windows, and now ApPa — perform **software task switching** instead, using the TSS only for the `ss0:esp0` stack pointer. Software switching is faster, more flexible, and gives the OS full control over what is saved and restored.

**Key rule:** Every time we context-switch to a different task, we write that task's kernel stack top into `tss.esp0` via `tss_set_kernel_stack()`. If we forget, the next interrupt would use the wrong stack — instant crash.

### The Context Switch in Detail

The context switch is a small assembly routine (`task_switch` in `switch.asm`). It exploits the x86 calling convention: the caller has already saved EAX, ECX, EDX (caller-saved), so we only need to save and restore the four **callee-saved** registers (EBX, ESI, EDI, EBP) plus ESP.

```
task_switch(uint32_t *old_esp, uint32_t new_esp):

  OUTGOING TASK (Task A):                    INCOMING TASK (Task B):
  ┌─────────────────────────┐                ┌─────────────────────────┐
  │  ...caller frames...    │                │  ...caller frames...    │
  │  return address         │                │  return address         │
  │  push ebx  ◄── 1. save │                │  ebx   ──► pop ebx  4. │
  │  push esi               │                │  esi   ──► pop esi     │
  │  push edi               │                │  edi   ──► pop edi     │
  │  push ebp               │                │  ebp   ──► pop ebp     │
  │ ESP ──────────────────  │ 2. *old_esp=ESP│  ◄── ESP               │
  └─────────────────────────┘                └─────────────────────────┘
                                3. ESP = new_esp
                                5. ret → resumes Task B
```

Steps:
1. **Push** EBX, ESI, EDI, EBP onto Task A's stack
2. **Save** Task A's ESP into its TCB (`*old_esp = ESP`)
3. **Load** Task B's ESP from its TCB (`ESP = new_esp`)
4. **Pop** EBP, EDI, ESI, EBX from Task B's stack
5. **`ret`** — pops the return address from Task B's stack and jumps there

For a brand-new task, step 5 jumps to `task_wrapper()` because `task_create()` placed that address on the stack.

### The Ready Queue

Tasks are linked in a **circular singly-linked list**. The scheduler walks the ring starting from the current task's `next` pointer, skipping any DEAD or BLOCKED tasks, and picks the first READY task it finds.

```
    ┌──────────┐     ┌──────────┐     ┌──────────┐
    │  idle    │────►│  shell   │────►│  task_c  │──┐
    │ (READY)  │     │ (READY)  │     │ (READY)  │  │
    └──────────┘     └──────────┘     └──────────┘  │
         ▲                                           │
         └───────────────────────────────────────────┘
                     circular linked list
```

### Time Slicing

The PIT (configured at 100 Hz in Phase 8) fires IRQ0 every 10 ms. The timer handler calls `schedule()`, which decrements a per-task time-slice counter. When the counter reaches zero, a context switch occurs.

```
    Default time slice: 10 ticks = 100 ms

    │◄──── 100 ms ─────►│◄──── 100 ms ─────►│◄──── 100 ms ─────►│
    ├──── Task A ───────┤──── Task B ───────┤──── Task C ───────┤
    tick tick tick tick tick tick tick tick tick tick tick tick tick ...
    10   9    8    7    6    5    4    3    2    1    0→switch
```

### New Task Startup ("Fake Frame" Technique)

When `task_create()` builds a new task, that task has never executed before — there is no "saved state" to restore. The solution is to **pre-populate the stack** with a fake frame that looks exactly like what `task_switch` expects to pop:

```
Stack of a newly created task (grows downward):

  High address (esp0 = stack base + 4096)
  ┌──────────────────────────────────────┐
  │  (uint32_t) entry_fn       ← cdecl arg1 for task_wrapper  │
  │  (uint32_t) 0              ← fake return address (unused)  │
  │  (uint32_t) task_wrapper   ← 'ret' will jump here          │
  │  (uint32_t) 0              ← fake ebx                      │
  │  (uint32_t) 0              ← fake esi                      │
  │  (uint32_t) 0              ← fake edi                      │
  │  (uint32_t) 0              ← fake ebp                      │
  └──────────────────────────────────────┘
  Low address (esp saved in TCB points here)
```

When the scheduler first switches to this task, `task_switch` pops EBP/EDI/ESI/EBX and `ret`s into `task_wrapper(entry_fn)`. The entry function address is passed as a standard cdecl parameter on the stack — this avoids relying on a specific register (EBX) surviving the switch, which is more portable and easier to reason about.

`task_wrapper` enables interrupts (`sti`) because the context switch that delivered control here occurred inside a timer ISR with IF=0. Without this, the new task would run with interrupts permanently masked. After calling the entry function, the wrapper calls `task_exit()` to clean up.

---

## What Changed and Why

### Modified Files

#### `kernel/arch/gdt.h` — Expanded from 3 to 6 GDT entries
**Before:** 3 entries (null, kernel code, kernel data).
**After:** 6 entries (null, kernel code, kernel data, user code, user data, TSS).

**Why:** The TSS must live in a GDT descriptor so the CPU can find it via the Task Register (TR). User code/data segments (Ring 3, DPL=3) are added now to avoid reworking the GDT later when userspace is implemented. Named selector constants (`GDT_KERNEL_CODE_SEG`, `GDT_USER_DATA_SEG`, `GDT_TSS_SEG`, etc.) replace hard-coded magic numbers throughout the codebase.

**GDT layout after this phase:**

| Index | Selector | Name | DPL | Purpose |
|-------|----------|------|-----|---------|
| 0 | 0x00 | Null | — | Required by x86 |
| 1 | 0x08 | Kernel Code | 0 | Ring 0 code execution |
| 2 | 0x10 | Kernel Data | 0 | Ring 0 data/stack |
| 3 | 0x18 | User Code | 3 | Ring 3 code (future) |
| 4 | 0x20 | User Data | 3 | Ring 3 data/stack (future) |
| 5 | 0x28 | TSS | 3 | Task State Segment |

#### `kernel/arch/gdt.c` — New public API functions
- **`gdt_set_gate_ext()`** — Exposes the internal `gdt_set_gate()` so `tss.c` can install the TSS descriptor without duplicating GDT manipulation code.
- **`gdt_reload()`** — Re-computes the GDT pointer and calls `gdt_flush()`. Needed because the TSS is installed *after* `gdt_init()` runs, so the CPU must be told about the new table size.
- User code/data segments (entries 3–4) are populated with DPL=3 access bytes.
- Entry 5 is zeroed as a placeholder, later filled by `tss_init()`.

#### `kernel/sys/timer.c` — Scheduler hook point
The timer IRQ handler is the natural place to invoke `schedule()` for preemptive switching. After incrementing the tick counter, it calls `schedule()` which either decrements the time-slice counter and returns, or performs a full context switch.

#### `kernel/arch/irq.c` — EOI ordering fix
The End-Of-Interrupt (EOI) signal is now sent to the PIC **before** the handler runs, not after. This is critical because the timer handler can perform a context switch (`schedule()` → `task_switch()`), which means control may never return to the code *after* `interrupt_handlers[...]()`. If EOI is deferred until after the handler, and a context switch occurs, the PIC never un-masks the timer line — freezing all future timer interrupts. Sending EOI first is safe because interrupts are already disabled (IF=0) throughout the ISR stub.

### New Files

#### `kernel/arch/tss.h` / `kernel/arch/tss.c` — Task State Segment
Defines the 104-byte `tss_entry_t` structure matching the x86 hardware layout. Provides:
- `tss_init(ss0, esp0)` — zeroes the TSS, sets the kernel stack, installs the TSS descriptor in GDT entry 5, reloads the GDT, and executes `ltr` to load the Task Register.
- `tss_set_kernel_stack(esp0)` — Updates `tss.esp0`. Called on every context switch.

**Why a separate file?** The TSS is logically part of the CPU architecture layer (`kernel/arch/`) but is distinct from segmentation (GDT) and interrupts (IDT). It will grow if we add I/O permission bitmaps or debug trap support.

#### `kernel/arch/tss_flush.asm` — Load Task Register
A 3-instruction assembly routine that loads `0x2B` (GDT entry 5, RPL=3) into the TR register using the `ltr` instruction. This is a privileged operation that can only be done in assembly.

#### `kernel/task/task.h` — Task Control Block definition
Defines the central data structure for multitasking:

```c
typedef struct task {
    uint32_t        id;                     // Unique task identifier
    char            name[TASK_NAME_MAX];    // Human-readable name
    task_state_t    state;                  // READY / RUNNING / BLOCKED / DEAD

    uint32_t        esp;                    // Saved stack pointer
    uint32_t        esp0;                   // Kernel stack top (for TSS)
    uint32_t        *kernel_stack;          // Base of PMM-allocated stack page

    struct task     *next;                  // Circular ready-queue link
} task_t;
```

The state machine:
```
                 task_create()
                      │
                      ▼
    ┌─────────────────────────────────┐
    │            READY                │ ◄──── schedule() preempts running task
    └────────────┬────────────────────┘
                 │ schedule() picks this task
                 ▼
    ┌─────────────────────────────────┐
    │           RUNNING               │ ← Only one task is RUNNING at a time
    └──┬──────────────────────────┬───┘
       │                          │
       │ sched_yield() or         │ task_exit()
       │ schedule() time-slice    │
       ▼                          ▼
    ┌──────────┐            ┌──────────┐
    │  READY   │            │   DEAD   │ → task_reap() frees stack
    └──────────┘            └──────────┘
                 (future)
    ┌──────────┐
    │ BLOCKED  │ → waiting for I/O, timer, mutex
    └──────────┘
```

#### `kernel/task/task.c` — Task lifecycle management
- **Static pool allocator:** 64 TCB slots in a fixed array, avoiding a circular dependency on `kmalloc` (which itself will eventually need lock protection from the scheduler).
- **`task_create(entry, name)`:** Allocates a TCB slot, grabs a 4 KB page from the PMM for the kernel stack, builds the fake context frame, and inserts the task into the ready queue.
- **`task_wrapper(entry_addr)`:** Trampoline function that receives the entry point as a cdecl parameter, enables interrupts (`sti`), calls the entry, and catches the return with `task_exit()`.
- **`task_exit()`:** Marks the current task DEAD and yields — the task never resumes.
- **`task_reap()`:** Scans the pool for DEAD tasks, frees their kernel stack pages back to the PMM, and zeroes the TCB slot for reuse.

#### `kernel/task/switch.asm` — Context switch
The core 15-instruction routine that actually swaps execution between two tasks:
1. Push EBX, ESI, EDI, EBP (callee-saved)
2. Save `ESP` into `*old_esp` (outgoing task's TCB)
3. Load `ESP` from `new_esp` (incoming task's TCB)
4. Pop EBP, EDI, ESI, EBX
5. `ret` into the incoming task

**Why assembly?** The context switch directly manipulates ESP — something a C compiler cannot express. The function must follow a precise ABI contract so that the register save/restore interleaves correctly with the C calling convention.

#### `kernel/task/sched.h` / `kernel/task/sched.c` — Round-robin scheduler
- **Circular linked-list ready queue** — O(n) scan for next READY task, O(n) insertion (walk to tail). Sufficient for the small task counts in a hobby OS.
- **Bootstrap task:** `sched_init()` wraps the currently executing `main()` context into a `boot_task` TCB (ID 0, name "idle"). This is the only task whose stack was not PMM-allocated.
- **`schedule()`:** Called from the timer ISR. Decrements `slice_remaining`; when it hits zero, finds the next READY task and calls `switch_to()`.
- **`sched_yield()`:** Cooperative entry point — immediately finds the next READY task and switches.
- **`sched_enable()` / `sched_disable()`:** Guard flag so kernel init code runs without preemption.
- **Interrupt safety:** `sched_add_task()` and `sched_yield()` bracket queue mutations with `cli`/`sti`.

---

## Memory Layout Impact

Each task consumes:
- **1 TCB** from the 64-slot static pool (~56 bytes each, ~3.5 KB total for pool)
- **1 PMM page** (4 KB) for its kernel stack

With 64 max tasks: worst-case extra memory = 64 × 4 KB = **256 KB** of kernel stacks, all within the existing PMM pool (0x201000–0xF00000).

The bootstrap/idle task reuses the original boot stack at `0x9FC00` and does not consume a PMM page.

---

## Boot Sequence Changes

```
Before (single-threaded):                After (multitasked):
─────────────────────────                ────────────────────────
gdt_init()                               gdt_init()
idt_init()                               idt_init()
pic_remap()                              pic_remap()
pit_init()                               pit_init()
timer_init()                             timer_init()
kmalloc_init()                           kmalloc_init()
pmm_init()                               pmm_init()
paging_init()                            paging_init()
                                         tss_init(0x10, 0x9FC00)  ← NEW
ata_init()                               ata_init()
ramdisk_init() + fs_init()               ramdisk_init() + fs_init()
keyboard_init()                          keyboard_init()
shell_init()                             shell_init()
sti                                      sched_init()              ← NEW
run_all_tests()                          sti
while(1) {}                              run_all_tests()
                                         sched_enable()            ← NEW
                                         while(1) { hlt; reap; }   ← idle loop
```

Key additions:
1. **`tss_init()`** immediately after `gdt_init()` — the TSS must be loaded before any interrupt could cause a privilege switch.
2. **`sched_init()`** after all subsystems are ready — wraps the current context as the idle task.
3. **`sched_enable()`** once tasks are created — flips the preemptive scheduling flag so the timer ISR starts calling `schedule()`.

---

## New Capabilities

### What the kernel can now do that it couldn't before:

| Capability | Description |
|-----------|-------------|
| **Run multiple tasks** | Create up to 64 independent kernel tasks, each with its own stack and register state |
| **Preemptive scheduling** | The timer interrupt automatically switches between tasks every 100 ms — no task can monopolise the CPU |
| **Cooperative yielding** | Tasks can voluntarily give up the CPU with `sched_yield()` for lower-latency switching |
| **Task lifecycle** | Tasks are created (`task_create`), run, and destroyed (`task_exit`); dead tasks' resources are reclaimed (`task_reap`) |
| **Idle task** | The bootstrap context becomes a proper idle task that runs `hlt` when no other work is available, reducing CPU power consumption |
| **TSS-based stack switching** | The CPU correctly finds each task's kernel stack on interrupt, a prerequisite for Ring 3 userspace |
| **Ring 3 GDT segments** | User code and data segments are installed in the GDT, ready for future userspace processes |

### What this enables in future phases:

| Future Phase | Enabled By |
|-------------|------------|
| **Userspace (Ring 3)** | TSS `esp0` switching + user code/data GDT segments |
| **Syscalls (INT 0x80)** | TSS provides kernel stack on privilege transition |
| **Blocking I/O** | BLOCKED task state + scheduler skipping blocked tasks |
| **Sleep / wait** | Timer-based wakeup by moving tasks from BLOCKED → READY |
| **Per-process address spaces** | Swap CR3 during context switch (extend `switch_to()`) |
| **ELF loader** | Load program into new task's address space and schedule it |
| **Priority scheduling** | Replace round-robin with multi-level feedback queue |

---

## API Reference

### TSS (`kernel/arch/tss.h`)

| Function | Description |
|----------|-------------|
| `tss_init(kernel_ss, kernel_esp)` | Install TSS in GDT, load Task Register. Call once after `gdt_init()`. |
| `tss_set_kernel_stack(esp0)` | Update the kernel stack pointer in the TSS. Called on every context switch. |

### Task (`kernel/task/task.h`)

| Function | Description |
|----------|-------------|
| `task_create(entry, name)` | Spawn a new kernel task. Returns `task_t*` or NULL. |
| `task_exit()` | Terminate the current task and yield. Never returns. |
| `task_get_current()` | Return the currently running `task_t*`. |
| `task_reap()` | Free resources (kernel stacks) of all DEAD tasks. |

### Scheduler (`kernel/task/sched.h`)

| Function | Description |
|----------|-------------|
| `sched_init()` | Create the bootstrap task from the current context. Call once. |
| `sched_enable()` | Enable preemptive scheduling (timer ISR calls `schedule()`). |
| `sched_disable()` | Disable preemptive scheduling. |
| `sched_add_task(task)` | Insert a READY task into the circular ready queue. |
| `sched_yield()` | Cooperatively switch to the next ready task. |
| `schedule()` | Core scheduler decision; called from timer ISR. |
| `sched_get_current()` | Return the currently running task. |
| `sched_is_enabled()` | Returns 1 if preemptive scheduling is active. |
| `sched_remove_task(task)` | Unlink a DEAD task from the circular ready queue (called by `task_reap()`). |

### GDT Extensions (`kernel/arch/gdt.h`)

| Symbol | Value | Description |
|--------|-------|-------------|
| `GDT_KERNEL_CODE_SEG` | `0x08` | Kernel code segment selector |
| `GDT_KERNEL_DATA_SEG` | `0x10` | Kernel data segment selector |
| `GDT_USER_CODE_SEG` | `0x1B` | User code segment selector (RPL=3) |
| `GDT_USER_DATA_SEG` | `0x23` | User data segment selector (RPL=3) |
| `GDT_TSS_SEG` | `0x2B` | TSS segment selector (RPL=3) |
| `gdt_set_gate_ext()` | — | Public GDT entry setter (for TSS installation) |
| `gdt_reload()` | — | Re-load GDT pointer after adding entries |

---

## File Summary

| File | Status | Description |
|------|--------|-------------|
| `kernel/arch/gdt.h` | Modified | 3 → 6 GDT entries, selector constants, new API |
| `kernel/arch/gdt.c` | Modified | User segments, TSS placeholder, `gdt_set_gate_ext()`, `gdt_reload()` |
| `kernel/arch/tss.h` | **New** | TSS structure definition and API |
| `kernel/arch/tss.c` | **New** | TSS init, kernel stack update |
| `kernel/arch/tss_flush.asm` | **New** | `ltr` instruction to load Task Register |
| `kernel/task/task.h` | **New** | TCB struct, task states, task API |
| `kernel/task/task.c` | **New** | Task create, exit, reap, static pool allocator |
| `kernel/task/sched.h` | **New** | Scheduler API |
| `kernel/task/sched.c` | **New** | Round-robin scheduler, ready queue, bootstrap task |
| `kernel/task/switch.asm` | **New** | Low-level context switch (15 instructions) |
| `kernel/sys/timer.c` | Modified | Calls `schedule()` from IRQ0 handler |
| `kernel/sys/kernel_main.c` | Modified | Adds `tss_init()`, `sched_init()`, `sched_enable()` to boot sequence |
| `kernel/arch/irq.c` | Modified | EOI sent before handler to survive context switches |
| `shell/shell.c` | Modified | Added `tasktest` command for post-boot multitasking tests |
| `tests/test_multitask.c/h` | **New** | 6 multitasking tests (creation, context switch, preemption, interleaving, reap, 3-concurrent) |
| `makefile` | Modified | `kernel/task/*.asm` in build, `run-term` uses curses, `run-log` for serial |
