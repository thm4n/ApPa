# ApPa
Interactive x86 OS with keyboard input support!

## How to Build and Run

```bash
make clean && make run
```

The kernel will boot in QEMU and display:
```
ApPa Kernel v0.1
Initializing interrupt system...
  [OK] IDT initialized
  [OK] PIC remapped
  [OK] Keyboard initialized
  [OK] Interrupts enabled

System ready. Type something!
> 
```

**Try it:** Click in the QEMU window and start typing! The keyboard input is fully functional.
- Type letters, numbers, symbols
- Press Enter for newline
- Press Backspace to delete
- Press Tab for tab character

---

## Current Development Status
- ✅ **Phase 1: CPU Exception Handlers (ISRs 0-31)** - COMPLETED
  - IDT structure and initialization
  - 32 exception handlers for CPU faults
  - Common exception handler with error reporting
- ✅ **Phase 2: PIC Remapping** - COMPLETED
  - PIC port and constant definitions (`kernel/pic.h`)
  - PIC remap function to avoid IRQ/exception conflicts
  - EOI (End-Of-Interrupt) handler for interrupt acknowledgment
- ✅ **Phase 3: IRQ Handlers (ISRs 32-47)** - COMPLETED
  - 16 IRQ assembly stubs (IRQ0-15 → INT 32-47)
  - IRQ dispatcher with custom handler support
  - Automatic EOI acknowledgment to PIC
- ✅ **Phase 4: Keyboard Driver** - COMPLETED
  - Scancode to ASCII translation table (US QWERTY)
  - IRQ1 keyboard interrupt handler
  - Backspace support
- ✅ **Phase 5: Integration** - COMPLETED
  - IDT initialization with exception and IRQ handlers
  - PIC remapping during kernel startup
  - Keyboard driver initialization
  - Interrupts enabled globally
  - **FULLY FUNCTIONAL KEYBOARD INPUT!**

---

## Next Steps

### Phase 6: Dynamic Memory Allocator (Heap) - **NEXT**
**Priority:** High | **Estimated:** 1-2 days  
**Why first:** Foundation for all dynamic data structures. Required for shell, file systems, and future features.

**Implementation:**
- `kernel/kmalloc.c` / `kmalloc.h` - Heap allocator implementation
- Start with simple first-fit or bitmap allocator
- Functions: `kmalloc()`, `kfree()`, `kmalloc_status()` (for debugging)
- Define heap region (e.g., 1MB-2MB in physical memory)
- Track allocated/free blocks with metadata headers

### Phase 7: Simple Command Shell
**Priority:** High | **Estimated:** 1 day  
**Why:** Makes keyboard input actually useful! Good practice for string handling.

**Features:**
- Command buffer and parsing (using kmalloc for buffers)
- Built-in commands:
  - `help` - List available commands
  - `clear` - Clear screen
  - `echo <text>` - Print text
  - `mem` - Show memory allocation statistics
  - `uptime` - Show system uptime (after timer implementation)
- Command history (optional enhancement)

### Phase 8: Timer (PIT - IRQ0)
**Priority:** Medium | **Estimated:** 1 day  
**Why:** Track system uptime, foundation for multitasking scheduler.

**Implementation:**
- `drivers/timer.c` / `timer.h` - PIT driver
- Configure PIT (Programmable Interval Timer) on IRQ0
- Track ticks since boot
- Convert ticks to seconds/minutes for uptime display

### Phase 9: Physical Memory Manager
**Priority:** High | **Estimated:** 2-3 days  
**Why:** Proper RAM detection and tracking. Required before paging.

**Implementation:**
- Detect available RAM (multiboot info or fixed size detection)
- Page frame allocator (4KB pages)
- Bitmap or stack-based free page tracking
- Functions: `alloc_page()`, `free_page()`
- Integrate with existing heap allocator

### Phase 10: Paging / Virtual Memory (MMU)
**Priority:** High | **Estimated:** 3-5 days  
**Why:** Memory protection, process isolation, enables userspace programs.

**Implementation:**
- `kernel/paging.c` / `paging.h` - Page table management
- Identity mapping for kernel (virtual = physical)
- Enable CR0.PG bit to activate MMU
- Page fault handler (ISR 14)
- Separate address spaces for future processes

### Phase 11: File System & Disk I/O
**Priority:** Medium | **Estimated:** 5-7 days  
**Why:** Persistent storage, load programs from disk.

**Prerequisites:** All previous phases completed  
**Implementation:**
- `drivers/ata.c` / `ata.h` - ATA/IDE disk driver (PIO mode)
- Simple file system (RAM disk first, then FAT12 or custom FS)
- VFS layer: `open()`, `read()`, `write()`, `close()`
- Directory listing in shell (`ls` command)

### Future Enhancements
- **Multitasking:** Task scheduler, context switching, TSS
- **Userspace:** Ring 3 processes, syscalls (INT 0x80)
- **ELF Loader:** Load and execute programs from disk
- **Networking:** NE2000 driver, basic TCP/IP stack
- **Graphics:** VESA VBE framebuffer mode

---

## Known Issues & Solutions

### Kernel Size vs Bootloader Sector Count

#### The Bug
The bootloader uses BIOS INT 13h to load the kernel from disk. It reads a fixed number of 512-byte sectors into memory at `0x1000`. If the kernel grows larger than the allocated sectors, the excess code is never loaded from disk.

**Symptoms:**
- Triple fault / reboot loop
- QEMU debug shows `check_exception old: 0xd new 0xd` (GPF → GPF → Double Fault)
- Crash at addresses well above `0x1000` (unloaded code region)

**Root cause:** Kernel binary exceeded the hardcoded sector count in `boot_sector.asm`.

#### Current Solution (Auto-Patching)
The makefile automatically:
1. Calculates required sectors from `kernel.bin` size
2. Patches the sector count byte in `boot_sector.bin` at build time
3. Pads `image.bin` to match

```makefile
SECTOR_PATCH_OFFSET = 0x142  # Location of 'mov dh, X' immediate
```

The bootloader contains a placeholder:
```asm
KERNEL_SECTORS_PATCH:          ; Label for makefile to find offset
    mov dh, 0                  ; PATCHED BY MAKEFILE
```

**Limit:** 63 sectors (~32KB). The build will fail if exceeded.

#### Future Solution: Two-Stage Bootloader
When the kernel exceeds 32KB, implement a two-stage bootloader:

```
Stage 1 (boot_sector.bin, 512 bytes):
  - Loaded by BIOS at 0x7C00
  - Loads Stage 2 from disk

Stage 2 (stage2.bin, unlimited size):
  - Can use LBA addressing for large disks
  - Can read multiple chunks to load large kernels
  - Loads kernel.bin at 0x1000
  - Switches to protected mode
  - Jumps to kernel

Image layout:
  [boot_sector.bin][stage2.bin][kernel.bin]
```

**Files to add:**
- `boot/stage2.asm` - Second stage loader
- Update `makefile` to build and concatenate stage2

**When to implement:** When `kernel.bin` approaches 32KB.

#### Recalculating the Patch Offset
If `boot_sector.asm` changes significantly, recalculate the offset:
```bash
nasm -f bin boot/boot_sector.asm -o /dev/null -l /dev/stdout | grep KERNEL_SECTORS_PATCH
# Or: hexdump -C bin/boot_sector.bin | grep "b6 00"
```
Update `SECTOR_PATCH_OFFSET` in `makefile` accordingly.
