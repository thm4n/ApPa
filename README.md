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
