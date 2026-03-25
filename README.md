# ApPa

A bare-metal x86 operating system built from scratch — two-stage bootloader, protected mode kernel, virtual memory, filesystem, and interactive shell.

## Quick Start

```bash
# Graphical QEMU window (use keyboard in the QEMU window)
make clean && make run

# Terminal mode with curses display (keyboard works)
make clean && make run-term

# Serial logging mode (output to stdout, no keyboard)
make clean && make run-log

# Debug with GDB
make debug
```

### Boot Output

```
ApPa Kernel v0.1
Initializing system...
  [OK] GDT initialized
  [OK] IDT initialized
  [OK] PIC remapped
  [OK] PIT initialized (100Hz)
  [OK] Timer initialized
  [OK] Kernel heap initialized
  [OK] Physical memory manager initialized
  [OK] Paging enabled (identity map 0-16MB)
  [OK] TSS initialized
  [OK] Kernel logging initialized
  [OK] ATA disk detected: QEMU HARDDISK
  [OK] RAM disk initialized (256 KB)
  [OK] SimpleFS mounted
  [OK] Keyboard initialized
  [OK] Shell initialized
  [OK] Scheduler initialized
  [OK] Syscall interface initialized (INT 0x80)
  [OK] Interrupts enabled

  ... unit tests run (7 checkpoints, 47 assertions) ...

  [OK] Preemptive scheduling enabled
=== ApPa OS Ready ===
> 
```

### Shell Commands

| Command | Description |
|---------|-------------|
| `help` | List available commands |
| `clear` | Clear the screen |
| `echo <text>` | Print text to screen |
| `mem` | Display heap allocation statistics |
| `pmem` | Display physical memory statistics |
| `uptime` | Show system uptime |
| `pagedir` | Display page directory info |
| `ls` | List files and directories |
| `cat <file>` | Display file contents |
| `write <file> <text>` | Write text to a file |
| `mkdir <name>` | Create a directory |
| `rm <name>` | Delete a file or directory |
| `disk` | Show ATA disk information |
| `tasktest` | Run multitasking tests (Phase 12) |
| `usertest` | Run Ring 3 userspace tests (Phase 13) |
| `color <name>` | Change text color (16 VGA colors) |

---

## Project Structure

```
ApPa/
├── boot/                   # Two-stage bootloader
│   ├── boot_sector.asm     #   Stage 1: loaded by BIOS at 0x7C00, loads stage2
│   ├── stage2.asm          #   Stage 2: loads kernel, switches to 32-bit protected mode
│   ├── kernel_entry.asm    #   Entry stub: calls main()
│   ├── 32bit-gdt.asm       #   Boot-time GDT definition
│   ├── switch_32pm.asm     #   Real mode → protected mode switch
│   ├── print.asm           #   BIOS teletype print (real mode)
│   ├── print_hex.asm       #   Hex value print (real mode)
│   ├── print_32pm.asm      #   VGA print (protected mode)
│   ├── disk_load.asm       #   BIOS INT 13h disk read
│   └── dev_setup.asm       #   Device setup helpers
│
├── kernel/
│   ├── arch/               # CPU & hardware architecture
│   │   ├── gdt.c/h         #   Global Descriptor Table setup
│   │   ├── gdt_flush.asm   #   GDT register load (lgdt)
│   │   ├── idt.c/h         #   Interrupt Descriptor Table setup
│   │   ├── idt_load.asm    #   IDT register load (lidt)
│   │   ├── isr.c/h         #   CPU exception handlers (ISR 0-31)
│   │   ├── isr_stubs.asm   #   ISR assembly entry points
│   │   ├── irq.c/h         #   Hardware interrupt handlers (IRQ 0-15)
│   │   ├── irq_stubs.asm   #   IRQ assembly entry points
│   │   ├── pic.c/h         #   8259 PIC driver (remap, EOI, mask)
│   │   ├── pit.c/h         #   8253 PIT driver (timer hardware)
│   │   ├── tss.c/h         #   Task State Segment (Ring 0 stack for interrupts)
│   │   ├── tss_flush.asm   #   Load Task Register (ltr)
│   │   ├── syscall.c/h     #   INT 0x80 dispatch table, syscall handlers, GPF handler
│   │   └── syscall_stub.asm #  INT 0x80 assembly entry point
│   ├── mem/                # Memory management
│   │   ├── kmalloc.c/h     #   Kernel heap allocator (best-fit, 1-2MB)
│   │   ├── pmm.c/h         #   Physical memory manager (bitmap, 4KB pages)
│   │   └── paging.c/h      #   Virtual memory (two-level page tables, identity map)
│   ├── task/               # Multitasking (Phase 12-13)
│   │   ├── task.c/h        #   Task Control Block, create/exit/reap, user-mode tasks
│   │   ├── sched.c/h       #   Round-robin preemptive scheduler
│   │   ├── switch.asm      #   Low-level context switch (callee-saved regs)
│   │   └── umode.asm       #   User-mode entry trampoline (iret to Ring 3)
│   └── sys/                # Core kernel services
│       ├── kernel_main.c   #   Kernel entry point and initialization sequence
│       ├── timer.c/h       #   System timer (IRQ0, uptime tracking)
│       └── klog.c/h        #   Kernel log buffer (circular, leveled)
│
├── drivers/                # Hardware device drivers
│   ├── screen.c/h          #   VGA text mode driver (80×25, 16 colors, scrolling)
│   ├── keyboard.c/h        #   PS/2 keyboard driver (IRQ1, US QWERTY)
│   ├── serial.c/h          #   Serial port driver (COM1, used for -nographic)
│   ├── ports.c/h           #   x86 I/O port access (inb, outb, word I/O)
│   └── ata.c/h             #   ATA PIO driver (LBA28, IDENTIFY, read/write)
│
├── fs/                     # Filesystem layer
│   ├── block.h             #   Block device interface (function pointers)
│   ├── ramdisk.c/h         #   PMM-backed RAM disk (256KB)
│   └── simplefs.c/h        #   SimpleFS (superblock, flat directory, contiguous alloc)
│
├── shell/                  # Interactive command shell
│   ├── shell.c             #   Command parsing, dispatch, and all command handlers
│   └── shell.h             #   Shell interface (init, input, execute)
│
├── libc/                   # Freestanding C library
│   ├── stdint.h            #   Fixed-width integer types
│   ├── stddef.h            #   size_t, NULL
│   ├── stdarg.h            #   va_list, va_start, va_arg, va_end
│   ├── string.c/h          #   String/memory functions (strlen, strcmp, memcpy, memcmp, ...)
│   ├── stdio.c/h           #   kprintf (formatted output to VGA)
│   └── syscall.c/h         #   User-side INT 0x80 wrappers (sys_write, sys_exit, ...)
│
├── tests/                  # Unit test suite (47+ assertions, 7 checkpoints)
│   ├── tests.c/h           #   Test runner and master header
│   ├── test_varargs.c/h    #   va_list system tests
│   ├── test_printf.c/h     #   kprintf format specifier tests (10 test groups)
│   ├── test_scroll_log.c/h #   Kernel log tests (currently skipped)
│   ├── test_pmm.c/h        #   Physical memory manager tests (15 tests)
│   ├── test_paging.c/h     #   Paging subsystem tests (8 tests)
│   ├── test_ata.c/h        #   ATA PIO driver tests (5 tests)
│   ├── test_fs.c/h         #   SimpleFS filesystem tests (10 tests)
│   ├── test_multitask.c/h  #   Multitasking tests (6 tests, run via `tasktest` command)
│   └── test_userspace.c/h  #   Ring 3 userspace tests (run via `usertest` command)
│
├── Information/            # Design documents and implementation guides
├── makefile                # Build system (cross-compiler, auto-patch sector count)
└── README.md               # This file
```

---

## Development Phases

| # | Phase | Key Files | Status |
|---|-------|-----------|--------|
| 1 | CPU Exception Handlers (ISR 0-31) | `kernel/arch/isr.c`, `isr_stubs.asm` | ✅ Done |
| 2 | PIC Remapping | `kernel/arch/pic.c` | ✅ Done |
| 3 | IRQ Handlers (IRQ 0-15) | `kernel/arch/irq.c`, `irq_stubs.asm` | ✅ Done |
| 4 | PS/2 Keyboard Driver | `drivers/keyboard.c` | ✅ Done |
| 5 | Integration & Interrupts | `kernel/arch/idt.c` | ✅ Done |
| 6 | Dynamic Heap Allocator | `kernel/mem/kmalloc.c` | ✅ Done |
| 7 | Command Shell | `shell/shell.c` | ✅ Done |
| 8 | System Timer (PIT/IRQ0) | `kernel/arch/pit.c`, `kernel/sys/timer.c` | ✅ Done |
| 9 | Physical Memory Manager | `kernel/mem/pmm.c` | ✅ Done |
| 10 | Paging / Virtual Memory | `kernel/mem/paging.c` | ✅ Done |
| 11 | File System & Disk I/O | `drivers/ata.c`, `fs/simplefs.c`, `fs/ramdisk.c` | ✅ Done |
| 12 | Multitasking | `kernel/task/sched.c`, `task.c`, `switch.asm`, `kernel/arch/tss.c` | ✅ Done |
| 13 | Userspace (Ring 3) | `kernel/arch/syscall.c`, `syscall_stub.asm`, `kernel/task/umode.asm`, `libc/syscall.c` | ✅ Done |
| 14 | Per-Process Address Spaces | — | ⬜ Next |

### Future Directions

- **Per-Process Address Spaces** — Private page directories, CR3 switching (Phase 14, planned)
- **ELF Loader** — Load and execute programs from disk
- **Blocking I/O / IPC** — Sleep queues, pipes, message passing
- **Networking** — NE2000 driver, basic TCP/IP stack
- **Graphics** — VESA VBE framebuffer mode

---

## Memory Layout

```
Physical Address          Usage
─────────────────────────────────────────────
0x00000000 - 0x000005FF   Interrupt Vector Table / BIOS Data
0x00000600 - 0x000007FF   Stage 2 bootloader (loaded here)
0x00001000 - 0x0000XXXX   Kernel code (loaded by stage2)
     ...                  (free — stack grows down from below)
0x0009FC00                Protected-mode stack top (ESP)
0x000A0000 - 0x000BFFFF   VGA video memory
0x000C0000 - 0x000FFFFF   BIOS ROM / reserved
0x00100000 - 0x001FFFFF   Kernel heap (kmalloc, 1 MB)
0x00200000                PMM bitmap (512 bytes for 4096 frames)
0x00201000 - 0x00EFFFFF   PMM page pool (~13 MB of allocatable pages)
0x00F00000 - 0x00FFFFFF   Reserved (device memory)
```

Real-mode bootloader stack: `SS:SP = 0x7000:0x0000` (physical 0x70000).

---

## Build System

### Requirements

- `i686-elf-gcc` / `i686-elf-ld` cross-compiler (at `~/.Code/CrossCompiler/i686_elf/bin/`)
- `nasm` assembler
- `qemu-system-i386`
- `python3` (for sector count patching)

### How It Works

The makefile builds a raw disk image with three concatenated parts:

```
[boot_sector.bin (512 B)] [stage2.bin (2 KB)] [kernel.bin (~45 KB)]
```

1. **boot_sector.asm** → 512-byte MBR loaded by BIOS at 0x7C00
2. **stage2.asm** → Loaded by stage 1 at 0x0600; loads the kernel into 0x1000 via BIOS INT 13h, switches to 32-bit protected mode, jumps to kernel
3. **kernel.bin** → Flat binary linked at 0x1000

The kernel sector count is **auto-patched** into `stage2.bin` at offset `STAGE2_PATCH_OFFSET` (0x3F) during the build so the bootloader always loads the correct number of sectors.

### Make Targets

| Target | Description |
|--------|-------------|
| `make build` | Compile and link (no QEMU) |
| `make run` | Build + launch QEMU with graphical window |
| `make run-term` | Build + launch QEMU with curses display (terminal VGA + keyboard) |
| `make run-log` | Build + launch QEMU with serial to stdout (no keyboard, tee'd to last_run.log) |
| `make debug` | Build + launch QEMU + connect GDB |
| `make clean` | Remove all build artifacts |

---

## Testing

Tests run automatically during kernel boot (after all subsystems initialize, before the shell prompt). See [tests/README.md](tests/README.md) for the test framework guide.

**Current results:** 47 `[PASS]` assertions across 7 checkpoints at boot, plus `tasktest` (6 multitasking tests) and `usertest` (3 Ring 3 syscall/GPF tests) available as shell commands.
