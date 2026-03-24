# Phase 10: Paging / Virtual Memory

## Overview
Enable the x86 Memory Management Unit (MMU) by building a two-level page table structure, identity-mapping the first 16 MB of physical RAM, and activating paging via CR0. This gives the kernel hardware-enforced address translation — the foundation for memory protection, process isolation, and higher-half kernel mappings in later phases.

**Goal:** Identity-map 0–16 MB, enable paging, handle page faults (ISR 14), expose a `pagedir` shell command.

**Why we need this:**
- **Address translation** — The MMU translates every virtual address through page tables before it reaches the physical bus
- **Memory protection** — Pages can be marked read-only, supervisor-only, or not-present; hardware enforces these on every access
- **Process isolation** — Each process can have its own page directory (CR3 swap) so it cannot touch another process's memory
- **Demand paging** — Pages can be marked not-present and allocated lazily on fault
- **Foundation for userspace** — User programs require Ring 3 page permissions to separate kernel from user memory

---

## Theory

### Virtual Address Breakdown

On x86 with 4 KB pages, every 32-bit virtual address is split into three fields:

```
   Virtual Address (32 bits)
   ┌────────────┬────────────┬──────────────┐
   │ PD Index   │ PT Index   │ Offset       │
   │ bits 31-22 │ bits 21-12 │ bits 11-0    │
   │ (10 bits)  │ (10 bits)  │ (12 bits)    │
   └─────┬──────┴─────┬──────┴──────┬───────┘
         │            │             │
         v            v             v
   Page Directory  Page Table   Byte within
   entry (1024)   entry (1024)  4 KB page
```

- **PD Index** (bits 31–22): selects one of 1024 entries in the page directory.
- **PT Index** (bits 21–12): selects one of 1024 entries in the selected page table.
- **Offset** (bits 11–0): byte offset within the selected 4 KB page frame.

### Two-Level Translation Walk

```
CR3 ──► Page Directory (4 KB, 1024 entries)
           │
           │ PD Index selects entry
           ▼
         PDE ──► Page Table (4 KB, 1024 entries)
                   │
                   │ PT Index selects entry
                   ▼
                 PTE ──► Physical Frame Base Address
                           │
                           │ + Offset (12 bits)
                           ▼
                     Physical Address
```

The CPU performs this walk on every memory access. The TLB (Translation Lookaside Buffer) caches recent translations so the walk rarely hits memory in practice.

### Page Directory Entry (PDE) Format

```
Bit(s)  Name            Meaning
──────  ──────────────  ────────────────────────────────────────────────
31-12   Frame Address   Physical address of page table (top 20 bits)
11-8    Available       Free for OS use (we don't use these yet)
7       PS              Page Size: 0 = 4 KB pages, 1 = 4 MB pages
6       (reserved)      Must be 0
5       A               Accessed — set by CPU on any access to this region
4       PCD             Page Cache Disable
3       PWT             Page Write-Through
2       U/S             User/Supervisor: 0 = Ring 0 only, 1 = Ring 3 allowed
1       R/W             Read/Write: 0 = read-only, 1 = writable
0       P               Present: 0 = not present (triggers #PF), 1 = valid
```

### Page Table Entry (PTE) Format

Same layout as PDE except:
- Bits 31-12 point to the **physical 4 KB frame** (not another table).
- Bit 6 is the **Dirty** flag — CPU sets it when this page is written to.
- Bit 7 is the PAT bit (ignored in this phase).

### Control Registers

| Register | Purpose |
|----------|---------|
| CR3 | Holds physical address of current page directory. Writing CR3 flushes the entire TLB. |
| CR0 | Bit 31 (PG) enables paging. Bit 0 (PE) must already be set (protected mode). |
| CR2 | Populated by CPU on page fault — contains the faulting virtual address. |

**Enable paging sequence:**
```nasm
mov eax, page_directory_phys   ; physical address of page directory
mov cr3, eax                   ; load page directory base register
mov eax, cr0
or  eax, 0x80000000            ; set PG bit (bit 31)
mov cr0, eax                   ; paging is NOW active
; next instruction is fetched via virtual addressing
```

### TLB Management

The TLB caches virtual → physical translations. It must be invalidated when page mappings change:

- **Full flush**: Reload CR3 (`mov eax, cr3 ; mov cr3, eax`).
- **Single-page flush**: `invlpg [virtual_address]` (486+, more efficient).

### Identity Mapping

An identity map sets `virtual address == physical address` for a memory region. This is the safest first step because:

1. The kernel was linked at physical address `0x1000` (`-Ttext 0x1000`).
2. All existing pointers (VGA at `0xB8000`, heap at `0x100000`, PMM bitmap at `0x200000`) keep working.
3. The instruction *immediately after* the CR0 write executes at the same address in EIP — no jump discontinuity.
4. The kernel can continue using physical addresses directly until a higher-half remap is introduced.

### Memory Cost

Covering 16 MB with 4 KB pages:
- 1 page directory: 4 KB (1024 × 4 bytes)
- 4 page tables (each covers 4 MB): 4 × 4 KB = 16 KB
- **Total: 20 KB** (5 physical frames from PMM)

---

## Memory Map (Updated for Phase 10)

```
Physical Address    Size       Purpose
──────────────────  ─────────  ──────────────────────────────────
0x00000000          4 KB       Real-mode IVT + BIOS data (reserved)
0x00001000          ~30 KB     Kernel code (.text, linked here)
0x00007C00          512 B      Boot sector (overwritable after boot)
0x00007E00          2 KB       Stage 2 bootloader (overwritable)
0x0009FC00          1 KB       Kernel stack (grows downward)
0x000B8000          4 KB       VGA text framebuffer
0x00100000          1 MB       Kernel heap (kmalloc)
0x00200000          512 B      PMM bitmap
0x00201000          ~13 MB     PMM allocation pool
    └─ 5 frames     20 KB     Page directory + 4 page tables (allocated by paging_init)
0x00F00000          1 MB       Device / reserved memory
```

The page directory and page tables are allocated from the PMM pool at runtime, so their exact physical addresses depend on allocation order.

---

## Implementation

### Files Created

| File | Purpose |
|------|---------|
| `kernel/paging.h` | Page table data structures, flags, and API declarations |
| `kernel/paging.c` | Identity mapping, CR3/CR0 manipulation, page fault handler, map/unmap/translate |
| `tests/test_paging.h` | Test function declaration |
| `tests/test_paging.c` | 8 unit tests for paging subsystem |

### API

```c
void     paging_init(void);                             // Identity map 0-16MB, enable CR0.PG
void     page_fault_handler(registers_t* regs);         // ISR 14 — diagnostic + halt
void     paging_map_page(uint32_t virt, uint32_t phys,
                         uint32_t flags);               // Map a single 4 KB page
void     paging_unmap_page(uint32_t virt);              // Unmap a single page + invlpg
uint32_t paging_translate(uint32_t virt);               // Walk tables, return phys or 0
void     paging_status(void);                           // Print PD summary (shell command)
```

### `paging_init()` Flow

1. Allocate page directory frame from PMM (`alloc_page()`). Zero it.
2. For each 4 MB chunk in [0, 16 MB):
   - Allocate a page table frame. Zero it.
   - Fill 1024 PTEs: `entry[i] = (chunk_base + i * 0x1000) | PRESENT | WRITABLE`.
   - Set PDE: `pd->entries[idx] = pt_phys | PRESENT | WRITABLE`.
3. Register `page_fault_handler` on ISR 14.
4. Load CR3 with page directory physical address.
5. Set CR0 bit 31 (PG) — paging is now active.

### `page_fault_handler()` Behavior

1. Read faulting address from CR2.
2. Decode error code bits:
   - Bit 0: 0 = page not present, 1 = protection violation
   - Bit 1: 0 = read access, 1 = write access
   - Bit 2: 0 = supervisor mode, 1 = user mode
3. Print diagnostic message with faulting address, error type, and EIP.
4. Halt — no recovery in this phase.

### Integration Points

- **`kernel/kernel_main.c`**: Call `paging_init()` after `pmm_init()`, before keyboard/shell init.
- **`kernel/shell.c`**: Add `pagedir` command that calls `paging_status()`.
- **`tests/tests.c`**: Add `test_paging()` to the test runner.

---

## Testing Strategy

| # | Test Name | What It Verifies |
|---|-----------|-----------------|
| 1 | Identity map kernel | `paging_translate(0x1000) == 0x1000` |
| 2 | Identity map VGA | `paging_translate(0xB8000) == 0xB8000` |
| 3 | Identity map heap | `paging_translate(0x100000) == 0x100000` |
| 4 | Identity map PMM pool | `paging_translate(0x201000) == 0x201000` |
| 5 | Boundary: last page | `paging_translate(0xFFF000) == 0xFFF000` |
| 6 | Unmapped returns 0 | `paging_translate(0x1000000) == 0` (above 16 MB) |
| 7 | Map then translate | Map new page at 0x2000000, verify translate succeeds |
| 8 | Unmap then translate | Unmap the page, verify translate returns 0 |

---

## Shell Command

```
> pagedir
Page Directory Status:
  Present entries: 4 / 1024
  Mapped pages: 4096
  Mapped memory: 16 MB (identity mapped 0x00000000 - 0x00FFFFFF)
```

---

## Common Pitfalls

| Pitfall | Explanation |
|---------|-------------|
| Not mapping the region containing EIP | CR0.PG enables paging instantly — the next instruction fetch uses virtual addressing. If EIP's page isn't mapped, triple fault. |
| Page directory/table not aligned | Must be 4096-byte aligned. `alloc_page()` returns aligned addresses — safe. |
| Physical vs virtual pointer confusion | After paging, C pointers are virtual. With identity mapping they equal physical, but the distinction matters for non-identity mappings later. |
| Uninitialized page tables | Random PTE bits → spurious Present entries → wrong mappings or faults. Always zero. |
| Stale TLB entries | After modifying a PTE, always `invlpg` or reload CR3. |
| Accidentally setting PS bit (bit 7) | Makes CPU treat PDE as 4 MB page instead of page table pointer. Keep bit 7 clear. |

---

## What This Enables Next

- **Per-process address spaces**: Swap CR3 on context switch
- **Demand paging**: Allocate physical frames lazily on page fault
- **Memory protection**: `PAGE_USER` flag separates kernel/user pages in hardware
- **Higher-half kernel**: Remap kernel to `0xC0000000+`, give userspace the lower 3 GB
- **Copy-on-write**: Share physical pages, clone on write fault
