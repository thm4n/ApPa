# Phase 16 — ELF Loader

## Goal

Load and execute **statically-linked ELF32 executables** from the SimpleFS
filesystem as Ring 3 user processes with their own private address spaces.

This replaces the Phase 13/15 approach where user "programs" are C functions
compiled directly into the kernel binary. After this phase, self-contained
binaries can be written to disk and launched from the shell with `exec <file>`.

---

## Theory — The ELF Format

### What Is ELF?

**ELF (Executable and Linkable Format)** is the standard binary format for
executables, object files, shared libraries, and core dumps on Unix-like
systems. It was introduced by Unix System V and adopted by Linux, FreeBSD,
Solaris, and others. ELF replaced older formats like a.out and COFF.

An ELF file is a structured container with three main views:

```
┌─────────────────────────────────────┐
│           ELF Header (52 B)         │  ← File identification + table pointers
├─────────────────────────────────────┤
│      Program Header Table           │  ← Runtime view: segments to load
│  (array of Elf32_Phdr entries)      │
├─────────────────────────────────────┤
│                                     │
│         Section Data                │  ← .text, .rodata, .data, .bss, ...
│       (variable size)               │
│                                     │
├─────────────────────────────────────┤
│      Section Header Table           │  ← Link-time view: sections for linker
│  (array of Elf32_Shdr entries)      │     (not needed at runtime)
└─────────────────────────────────────┘
```

The **program header table** describes how to load the file into memory
(runtime view). The **section header table** describes the logical layout
for linking and debugging. An OS loader only needs the program headers.

### Why ELF?

- **Standard** — well-documented, widely used, supported by GCC/LD/GDB
- **Simple for static executables** — just walk PT_LOAD segments and map them
- **Extensible** — supports dynamic linking, shared libraries, TLS (future)
- **Metadata-rich** — entry point, architecture, endianness all in the header

### ELF32 Header Structure

The ELF header sits at byte offset 0 and is always 52 bytes for 32-bit ELF:

| Offset | Size | Field           | Our Expected Value          |
|--------|------|-----------------|-----------------------------|
| 0x00   | 4    | `e_ident` magic | `0x7f 'E' 'L' 'F'`         |
| 0x04   | 1    | `EI_CLASS`      | 1 (`ELFCLASS32` — 32-bit)   |
| 0x05   | 1    | `EI_DATA`       | 1 (`ELFDATA2LSB` — little-endian) |
| 0x06   | 1    | `EI_VERSION`    | 1 (`EV_CURRENT`)            |
| 0x07   | 9    | `EI_OSABI` + padding | (ignored)              |
| 0x10   | 2    | `e_type`        | 2 (`ET_EXEC` — executable)  |
| 0x12   | 2    | `e_machine`     | 3 (`EM_386` — Intel 80386)  |
| 0x14   | 4    | `e_version`     | 1                           |
| 0x18   | 4    | `e_entry`       | Virtual entry point address |
| 0x1C   | 4    | `e_phoff`       | Program header table offset |
| 0x20   | 4    | `e_shoff`       | Section header table offset |
| 0x24   | 4    | `e_flags`       | 0 (no processor-specific flags) |
| 0x28   | 2    | `e_ehsize`      | 52 (size of this header)    |
| 0x2A   | 2    | `e_phentsize`   | 32 (size of one phdr entry) |
| 0x2C   | 2    | `e_phnum`       | Number of program headers   |
| 0x2E   | 2    | `e_shentsize`   | Size of one section header  |
| 0x30   | 2    | `e_shnum`       | Number of section headers   |
| 0x32   | 2    | `e_shstrndx`    | String table section index  |

The magic bytes `\x7fELF` are the first thing any ELF parser checks. If they
don't match, the file is not ELF. The class/data/machine fields let us reject
64-bit, big-endian, or non-i386 binaries early.

### Program Headers (Segments)

Each entry in the program header table describes one **segment** — a
contiguous region of the file to load into a contiguous region of virtual
memory. The key segment type is `PT_LOAD`:

| Field      | Size | Meaning                                           |
|------------|------|---------------------------------------------------|
| `p_type`   | 4    | Segment type — `1 = PT_LOAD` (loadable)           |
| `p_offset` | 4    | Byte offset from start of file to segment data    |
| `p_vaddr`  | 4    | Virtual address where segment is mapped in memory |
| `p_paddr`  | 4    | Physical address (unused on modern systems)       |
| `p_filesz` | 4    | Number of bytes to copy from the file             |
| `p_memsz`  | 4    | Total bytes in memory (≥ `p_filesz`)              |
| `p_flags`  | 4    | Permission flags: `PF_R` (4), `PF_W` (2), `PF_X` (1) |
| `p_align`  | 4    | Alignment requirement (usually page-aligned)      |

When `p_memsz > p_filesz`, the excess bytes are **zeroed** — this is how
the `.bss` section (uninitialized global variables) works. The linker
places `.bss` at the end of a `PT_LOAD` segment and sets `p_memsz`
accordingly; the loader just allocates zero-filled pages.

A typical statically-linked executable has 1–2 `PT_LOAD` segments:

```
Segment 0:  .text + .rodata         (R-X, file-backed)
Segment 1:  .data + .bss            (RW-, partially file-backed)
```

Our `hello.elf` is small enough that everything fits in a single segment.

### The Loading Process (Theory)

When an OS loads an ELF executable:

1. **Read** the ELF header from the file
2. **Validate** magic, class, endianness, type, and machine
3. **Create** a new virtual address space (page directory)
4. **Walk** the program header table, for each `PT_LOAD` segment:
   - Allocate physical page frames covering `[p_vaddr .. p_vaddr + p_memsz)`
   - Zero all pages (guarantees BSS is zeroed)
   - Copy `p_filesz` bytes from file offset `p_offset` into the pages
   - Set page permissions based on `p_flags` (R/W/X → `PAGE_USER` + optional `PAGE_WRITABLE`)
5. **Set up a stack** — allocate pages at a fixed high virtual address
6. **Create the task** with `EIP = e_entry`, `ESP = top of stack`
7. **Switch to Ring 3** — the CPU begins executing user code at the entry point

This is conceptually similar to what Linux's `execve()` does (simplified:
no dynamic linking, no interpreter, no signal setup, no argv/envp yet).

### Virtual Memory and ELF

ELF relies on **virtual memory** to work correctly. Each process gets its own
page directory (Phase 15), so two processes can both map code at 0x08048000
without conflicting — they have different physical frames behind the same
virtual address.

The loader must decide where in virtual address space each segment goes. For
`ET_EXEC` (statically linked) binaries, the `p_vaddr` field is **absolute** —
the loader maps the segment exactly there. (For `ET_DYN` / PIE executables,
`p_vaddr` is a relative offset and the loader picks a base address — not
supported yet.)

### Security Note

Our loader currently maps all PT_LOAD segments as **read-write** if `PF_W`
is set, or **read-execute** otherwise. There is no hardware NX (No-Execute)
bit enforcement on i386 without PAE — all readable pages are implicitly
executable. True W^X (write XOR execute) enforcement would require PAE
paging with the NX bit, which is a future enhancement.

---

## Implementation Details

### New Files

| File | Purpose |
|------|---------|
| `kernel/exec/elf.h` | ELF32 type definitions (`elf32_ehdr_t`, `elf32_phdr_t`), all constants (`ELFCLASS32`, `EM_386`, `PT_LOAD`, `PF_X/W/R`, etc.), and public loader API |
| `kernel/exec/elf.c` | ELF header validator, `load_segments()` PT_LOAD mapper, `elf_exec()` and `elf_exec_mem()` entry points |
| `user/hello.c` | Minimal self-contained Ring 3 ELF test program — uses raw INT 0x80 syscalls to print a greeting and its PID, then exits |
| `user/link.ld` | Linker script for user programs — sets base virtual address to 0x08048000, discards `.comment`/`.note`/`.eh_frame` |
| `tests/test_elf.c` | Four test groups: validation, memory load, filesystem load, resource cleanup |
| `tests/test_elf.h` | Test header declaring `void test_elf(void)` |

### Modified Files

| File | Change |
|------|--------|
| `kernel/task/task.h` | Added `task_create_user_mapped()` declaration — creates a Ring 3 task from a pre-built page directory |
| `kernel/task/task.c` | Implemented `task_create_user_mapped()` — allocates kernel stack, stores the supplied page directory/CR3, builds iret frame (SS/ESP/EFLAGS/CS/EIP), adds task to scheduler |
| `shell/shell.c` | Added `exec <file>` command (loads ELF from FS), `elftest` command (runs ELF loader tests), added `stdio.h` and `test_elf.h` includes |
| `tests/tests.h` | Added `#include "test_elf.h"` |
| `makefile` | Excluded `user/` from kernel `C_SOURCES`; added user program build pipeline (compile → link → xxd embed); added `user-programs` target; updated `clean` to remove `user/*.o`, `user/*.elf`, `user/*_elf.h` |
| `boot/stage2.asm` | Fixed signed-comparison bug in sector-capping logic (`jle` → `jbe`) — see Bootloader Bug Fix below |
| `README.md` | Updated phase status table (14–16 all ✅ Done), added new shell commands, added `kernel/exec/` and `user/` to project structure, updated future directions |

---

## Virtual Address Layout

```
0x00000000 – 0x00FFFFFF   Kernel (16 MB identity map, supervisor-only)
                           PDE 0–3, not accessible from Ring 3

0x08048000 – 0x0FFFFFFF   User .text / .rodata / .data / .bss
                           Mapped per PT_LOAD segments from ELF
                           Private pages per process (via per-process PD)

0xBFFFF000 – 0xBFFFFFFF   User stack (1 page, mapped by loader)
                           PAGE_PRESENT | PAGE_WRITABLE | PAGE_USER

0xC0000000                 User ESP starts here (stack grows down)
```

User ELF binaries are linked at **0x08048000** — the traditional i386
default inherited from SVR4. This address was chosen historically to leave
the first 128 MB of virtual space available for the kernel and to align
with page boundaries for efficient mapping.

---

## Loading Flow

```
elf_exec("hello.elf", "hello")
  │
  ├─ fs_stat()          → get file size, reject if 0 or > 256 KB
  ├─ kmalloc()          → allocate temporary read buffer
  ├─ fs_read_file()     → read entire ELF into buffer
  │
  ├─ elf_validate()     → check header:
  │   ├─ magic: 0x7f 'E' 'L' 'F'
  │   ├─ class: ELFCLASS32
  │   ├─ data:  ELFDATA2LSB (little-endian)
  │   ├─ type:  ET_EXEC (static executable)
  │   ├─ machine: EM_386
  │   ├─ phoff/phnum/phentsize present and valid
  │   └─ entry point ≠ 0
  │
  ├─ paging_clone_directory()  → new per-process page directory
  │   (clones kernel PDE 0 as private copy, shares PDE 1–3)
  │
  ├─ load_segments()    → for each PT_LOAD program header:
  │   ├─ Reject if p_vaddr overlaps kernel (< 16 MB)
  │   ├─ For each page in [p_vaddr .. p_vaddr + p_memsz):
  │   │   ├─ alloc_page()           → physical frame
  │   │   ├─ memset(phys, 0, 4096)  → zero entire page (BSS coverage)
  │   │   ├─ memcpy(phys+off, file_data, len)  → copy file data
  │   │   └─ paging_map_page_in(dir, vaddr, phys, flags)
  │   └─ Excess p_memsz beyond p_filesz is already zeroed = BSS
  │
  ├─ alloc_page()       → user stack physical page
  ├─ memset(0)          → zero the stack page
  ├─ paging_map_page_in(dir, USER_STACK_VIRT, stack_phys,
  │                      PAGE_PRESENT | PAGE_WRITABLE | PAGE_USER)
  │
  ├─ task_create_user_mapped(e_entry, name, dir, dir_phys)
  │   ├─ alloc_tcb()    → grab a free Task Control Block
  │   ├─ alloc_page()   → kernel stack (4 KB, supervisor-only)
  │   ├─ Store page_dir + cr3 in TCB
  │   ├─ Build iret frame on kernel stack:
  │   │     SS=0x23, ESP=0xC0000000, EFLAGS=0x202, CS=0x1B, EIP=e_entry
  │   ├─ Build task_switch frame:
  │   │     ret=enter_usermode, ebx=0, esi=0, edi=0, ebp=0
  │   └─ sched_add_task()
  │
  └─ kfree(buffer)      → free the temporary file buffer
```

### How iret Drops to Ring 3

When the scheduler first switches to the new task:

1. `task_switch` (switch.asm) pops ebp/edi/esi/ebx, then `ret` → lands in `enter_usermode`
2. `enter_usermode` (umode.asm) executes `iret`
3. CPU pops **EIP**, **CS** (0x1B = Ring 3 code segment), **EFLAGS**, **ESP**, **SS** (0x23 = Ring 3 data segment)
4. CPU switches to Ring 3 and begins executing at `e_entry` (e.g. 0x0804814b)
5. The user program's `_start()` runs in its private address space

---

## User Program Build Pipeline

User programs are built **separately** from the kernel by the Makefile:

```
user/hello.c
    │  i686-elf-gcc -ffreestanding -nostdlib -c
    ▼
user/hello.o
    │  i686-elf-ld -T user/link.ld
    ▼
user/hello.elf          ← standalone ELF32 executable at 0x08048000
    │  xxd -i
    ▼
user/hello_elf.h        ← C byte array embedded in kernel for testing
```

The kernel's `C_SOURCES` explicitly excludes `user/*.c` so user programs
are never compiled into the kernel binary. The generated `hello_elf.h`
is included by `tests/test_elf.c` to embed the binary for automated testing.

Makefile rules:

```makefile
# User C → object
$(USER_DIR)/%.o: $(USER_DIR)/%.c
    $(CC) -g -ffreestanding -nostdlib -march=i686 -c $< -o $@

# User object → ELF (using user linker script)
$(USER_DIR)/%.elf: $(USER_DIR)/%.o $(USER_DIR)/link.ld
    $(LD) -T $(USER_DIR)/link.ld $< -o $@

# ELF → embeddable C header
$(USER_DIR)/%_elf.h: $(USER_DIR)/%.elf
    xxd -i $< > $@
```

### Linker Script (user/link.ld)

```ld
ENTRY(_start)
SECTIONS {
    . = 0x08048000;          /* Traditional i386 user base address */
    .text   : ALIGN(4096) { *(.text*)   }
    .rodata : ALIGN(4)    { *(.rodata*) }
    .data   : ALIGN(4096) { *(.data*)   }
    .bss    : ALIGN(4)    { *(.bss*) *(COMMON) }
    /DISCARD/ : { *(.comment) *(.note*) *(.eh_frame*) }
}
```

### Writing a User Program

1. Create `user/myprogram.c` with a `void _start(void)` entry point
2. Use INT 0x80 inline assembly for syscalls (ABI: EAX=number, EBX=arg1, ECX=arg2)
3. Available syscalls: `SYS_EXIT` (0), `SYS_WRITE` (1), `SYS_READ` (2), `SYS_YIELD` (3), `SYS_GETPID` (4), `SYS_SLEEP` (5)
4. Run `make build` — the Makefile auto-discovers and builds all `user/*.c`
5. Write the `.elf` file to SimpleFS and run with `exec`

### Example: hello.c

```c
#define SYS_EXIT    0
#define SYS_WRITE   1
#define SYS_GETPID  4

static int syscall0(int num) {
    int ret;
    __asm__ volatile("int $0x80" : "=a"(ret) : "a"(num) : "memory");
    return ret;
}

static int syscall2(int num, int a1, int a2) {
    int ret;
    __asm__ volatile("int $0x80" : "=a"(ret) : "a"(num), "b"(a1), "c"(a2) : "memory");
    return ret;
}

void _start(void) {
    const char msg[] = "Hello from ELF!\n";
    syscall2(SYS_WRITE, (int)msg, sizeof(msg) - 1);

    int pid = syscall0(SYS_GETPID);
    /* ... print PID ... */

    syscall0(SYS_EXIT);
    for (;;);
}
```

The binary produced is ~462 bytes with a single PT_LOAD segment:

```
Type   Offset   VirtAddr   PhysAddr   FileSiz  MemSiz  Flg Align
LOAD   0x001000 0x08048000 0x08048000 0x001ce  0x001ce R E 0x1000
```

---

## API Reference

### kernel/exec/elf.h

```c
/* Load ELF from SimpleFS and spawn as Ring 3 task.
 * Returns: task pointer, or NULL on failure. */
task_t* elf_exec(const char *filename, const char *task_name);

/* Load ELF from a memory buffer and spawn as Ring 3 task.
 * Returns: task pointer, or NULL on failure. */
task_t* elf_exec_mem(const void *buf, uint32_t size, const char *task_name);

/* Validate an ELF32 i386 executable header.
 * Returns: 0 if valid, -1 if not. */
int elf_validate(const void *buf, uint32_t size);
```

### kernel/task/task.h (new function)

```c
/* Create Ring 3 task with a pre-built page directory.
 * The caller has already mapped code segments and user stack.
 * Allocates a kernel stack, builds iret frame, adds to scheduler.
 * Returns: task pointer, or NULL on failure. */
task_t* task_create_user_mapped(uint32_t entry_vaddr, const char *name,
                                 void *dir, uint32_t dir_phys);
```

### Validation Checks

The loader rejects ELF files that:

| Check | Reason |
|-------|--------|
| Magic ≠ `\x7fELF` | Not an ELF file |
| Class ≠ `ELFCLASS32` | 64-bit binary, we only support 32-bit |
| Data ≠ `ELFDATA2LSB` | Big-endian, x86 is little-endian |
| Type ≠ `ET_EXEC` | Relocatable/shared object, not a static executable |
| Machine ≠ `EM_386` | Wrong architecture (ARM, MIPS, etc.) |
| `e_phoff` = 0 or `e_phnum` = 0 | No program headers — nothing to load |
| `e_entry` = 0 | No entry point |
| `p_vaddr` < 16 MB | Segment overlaps kernel memory |
| File size > 256 KB | Safety limit (`ELF_MAX_FILE_SIZE`) |
| `p_filesz` > `p_memsz` | Corrupt segment header |
| Program header table out of bounds | Truncated file |

---

## Bootloader Bug Fix

During Phase 16 testing, the kernel grew to **150 sectors** (76 KB), which
triggered a latent **signed-comparison bug** in the stage 2 bootloader.

### Root Cause

In `boot/stage2.asm`, the sector-capping logic used **signed** branch
instructions (`jle` / assembles to `jng`) to compare unsigned byte values:

```asm
cmp al, [sectors_remaining]    ; unsigned values 0–255
jle .cap_ok                    ; BUG: jle = signed comparison
```

When `sectors_remaining = 150` (0x96), the **signed** 8-bit interpretation
is **−106**. The CMP instruction computes `58 − (−106)` in signed
arithmetic, concluding that 58 > −106, so `JLE` (jump if less or equal)
is **not taken**. The code falls through to `mov al, [sectors_remaining]`,
loading 150 into AL, and the bootloader attempts to read all 150 sectors
in a single BIOS INT 13h call — which the BIOS rejects.

### Fix

Changed to **unsigned** branch instructions:

```asm
cmp al, [sectors_remaining]
jbe .cap_ok                    ; ← JBE = unsigned "below or equal"

cmp al, 127
jbe .do_read                   ; ← JBE = unsigned
```

This bug is dormant for kernels ≤ 127 sectors (65,024 bytes). All previous
builds fit under this threshold, so it was never triggered until Phase 16
added enough code to push the kernel to 150 sectors.

### Signed vs Unsigned Branch Instructions (x86)

| Signed | Unsigned | Meaning |
|--------|----------|---------|
| `JL` / `JNGE` | `JB` / `JNAE` | Less than |
| `JLE` / `JNG` | `JBE` / `JNA` | Less than or equal |
| `JG` / `JNLE` | `JA` / `JNBE` | Greater than |
| `JGE` / `JNL` | `JAE` / `JNB` | Greater than or equal |

The signed variants check SF ⊕ OF (and ZF), while unsigned variants check
CF (and ZF). When comparing values that should be treated as positive
integers 0–255 (like sector counts), always use the unsigned variants.

---

## Shell Commands

| Command | Description |
|---------|-------------|
| `exec <file>` | Load an ELF binary from SimpleFS and spawn it as a Ring 3 task |
| `elftest` | Run the ELF loader test suite (4 test groups) |

### Example Session

```
> exec hello.elf
[ELF] Loading 'hello.elf'...
[ELF] Spawned task 'hello.elf' (tid=5)
Hello from ELF!
  My PID: 5
```

---

## Test Suite

Run via the `elftest` shell command. Four test groups:

| # | Test | Assertions |
|---|------|------------|
| 1 | **Validation** | NULL buffer → rejected, truncated buffer → rejected, bad magic → rejected, valid `hello.elf` → accepted |
| 2 | **Load from Memory** | `elf_exec_mem()` returns non-NULL task, task `is_user == 1`, task has private `cr3 ≠ 0`, task has `page_dir ≠ NULL` |
| 3 | **Load from Filesystem** | Write embedded ELF to SimpleFS → `elf_exec()` returns task, task is Ring 3; non-existent file → returns NULL |
| 4 | **Resource Cleanup** | PMM `get_free_memory()` before and after spawn+reap differ by ≤ 4096 bytes (1 page tolerance) |

---

## Build Stats

- **Sources:** 41 C + 8 ASM → 50 objects
- **Kernel size:** ~76 KB (150 sectors)
- **hello.elf:** ~462 bytes (1 PT_LOAD segment at 0x08048000)
- **Boot:** Verified via GDB — kernel reaches idle loop at `main+757: call task_reap`

---

## Future Extensions

- **argv/argc** — Push command-line arguments onto user stack before entry
- **SYS_EXEC** — Syscall to replace current process image with a new ELF
- **Larger user stacks** — Multi-page stack with guard pages
- **ELF shared libraries** — Dynamic linking with `ET_DYN` / `.interp` support
- **PIE support** — Position-independent executables with ASLR
- **NX enforcement** — PAE paging with NX bit for W^X security
