# Memory Configuration Guide

## Overview
All memory layout constants are now centralized in the **makefile** for easy configuration.

## Configuration Variables

### In Makefile

```makefile
# Memory layout configuration
KERNEL_OFFSET = 0x1000          # Where kernel code loads
STACK_BASE = 0x9FC00            # Protected mode stack top
REAL_MODE_STACK = 0x9000        # Real mode stack (bootloader)
```

## How to Change Stack Size

### Method 1: Edit Makefile (Recommended)

Simply change the `STACK_BASE` value in the makefile:

```makefile
# For more stack space:
STACK_BASE = 0x9FFF0            # ~608KB stack (maximum safe value)

# For less stack space (not recommended):
STACK_BASE = 0x98000            # ~544KB stack
```

### Method 2: Override at Build Time

```bash
# Build with custom stack size
make STACK_BASE=0x9FFF0 bin/image.bin

# Build with custom kernel offset
make KERNEL_OFFSET=0x2000 STACK_BASE=0x9FFF0 bin/image.bin
```

## Memory Layout

```
Address Range          | Usage                    | Size
-----------------------|--------------------------|--------
0x00000000 - 0x000007FF| Interrupt Vector Table   | 2KB
0x00000800 - 0x00000FFF| BIOS Data Area           | 2KB
0x00001000 - 0x0000XXXX| Kernel Code              | ~20KB
0x0000XXXX - 0x0009FBFF| Kernel Stack ⬇           | ~607KB
0x0009FC00 - 0x0009FFFF| Stack Base (ESP)         | 1KB
0x000A0000 - 0x000BFFFF| VGA Video Memory         | 128KB
0x000C0000 - 0x000FFFFF| BIOS ROM                 | 256KB
```

## Stack Size Calculation

```
Available Stack Space = STACK_BASE - (KERNEL_OFFSET + Kernel Size)

Current configuration:
  STACK_BASE = 0x9FC00 (639KB)
  KERNEL_OFFSET = 0x1000 (4KB)
  Kernel Size ≈ 20KB
  
  Stack Space = 0x9FC00 - (0x1000 + 20KB)
              = 639KB - 24KB
              = 615KB ≈ 607KB usable
```

## Safe Values

| STACK_BASE | Location | Stack Space | Notes |
|------------|----------|-------------|-------|
| `0x90000`  | 576KB    | ~544KB      | Original (too small) |
| `0x98000`  | 608KB    | ~576KB      | Moderate |
| `0x9FC00`  | 639KB    | ~607KB      | Current (safe) ✅ |
| `0x9FFF0`  | 640KB    | ~608KB      | Maximum safe |
| `0xA0000`  | 640KB    | ❌ INVALID  | VGA memory starts here! |

## Important Constraints

1. **Maximum STACK_BASE:** Must be < 0xA0000 (VGA memory boundary)
2. **Minimum STACK_BASE:** Must be > KERNEL_OFFSET + Kernel Size
3. **KERNEL_OFFSET:** Must match linker `-Ttext` value (handled automatically)
4. **Alignment:** Values should be aligned to 16 bytes for best performance

## How It Works

1. **Makefile defines constants:**
   ```makefile
   STACK_BASE = 0x9FC00
   ```

2. **Passes to NASM via -D flags:**
   ```makefile
   SMFLAGS = -D STACK_BASE=$(STACK_BASE) ...
   ```

3. **Assembly files use with fallback:**
   ```asm
   %ifndef STACK_BASE
       STACK_BASE equ 0x9FC00  ; Default if not provided
   %endif
   
   mov ebp, STACK_BASE
   mov esp, ebp
   ```

4. **Linker uses KERNEL_OFFSET:**
   ```makefile
   ${LD} -o kernel.bin -Ttext $(KERNEL_OFFSET) ...
   ```

## Verification

After changing values, rebuild and check:

```bash
# Rebuild from scratch
make clean && make bin/image.bin

# Verify constants were passed to NASM
make clean && make 2>&1 | grep "STACK_BASE"

# Should see:
# nasm -D KERNEL_OFFSET=0x1000 -D STACK_BASE=0x9FC00 ...
```

## Troubleshooting

### Stack Overflow Exception (#5)
- **Symptom:** "Bound Range Exceeded" exception
- **Cause:** STACK_BASE too low, not enough stack space
- **Fix:** Increase STACK_BASE (e.g., from 0x90000 to 0x9FC00)

### Kernel Won't Load
- **Symptom:** Blank screen or immediate reboot
- **Cause:** STACK_BASE conflicts with kernel code
- **Fix:** Ensure STACK_BASE > KERNEL_OFFSET + Kernel Size

### VGA Corruption
- **Symptom:** Garbled screen output
- **Cause:** STACK_BASE >= 0xA0000 (overwrites VGA memory)
- **Fix:** Lower STACK_BASE below 0xA0000
