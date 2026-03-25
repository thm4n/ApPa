# Phase 14: Persistent FS & ATA Block Device

## Overview
Replace the volatile RAM disk backing store with a persistent ATA disk so the filesystem survives reboots. This phase creates a thin `block_device_t` adapter over the existing ATA PIO driver, adds a `klog` flush-to-file capability for debug logging, and hardens `fs_delete` for recursive directory cleanup.

**Goal:** Wire the existing ATA driver as a `block_device_t`, boot with a QEMU disk image, and persist files (including kernel logs) across reboots.

**Why we need this:**
- **Persistence** — The RAM disk loses everything on reboot. An ATA-backed filesystem keeps files, logs, and configuration across power cycles.
- **Debug logging** — Kernel log messages (`klog`) can be flushed to a file on disk and inspected from the host (via raw disk image reads or from within the shell).
- **Block device validation** — The `block_device_t` abstraction was designed for exactly this swap. Proving it works with a real device validates the architecture.
- **Foundation for future work** — ELF loading, swap space, and crash logs all require persistent storage.

---

## Theory

### Current State (Phase 11)

```
                   ┌────────────────────┐
                   │     SimpleFS        │
                   │  (flat directory,   │
                   │   contiguous alloc) │
                   └────────┬───────────┘
                            │ block_device_t interface
                            │ read(block, buf) / write(block, buf)
                   ┌────────▼───────────┐
                   │     RAM Disk        │
                   │  (PMM-allocated     │
                   │   memory, volatile) │
                   └────────────────────┘
```

All filesystem data lives in RAM. A reboot erases everything.

### Target State (Phase 14)

```
                   ┌────────────────────┐
                   │     SimpleFS        │  ← unchanged
                   └────────┬───────────┘
                            │ block_device_t interface
                   ┌────────▼───────────┐
                   │  ATA Block Device   │  ← NEW adapter (5 lines)
                   │  ata_blockdev.c     │
                   └────────┬───────────┘
                            │ ata_read_sectors / ata_write_sectors
                   ┌────────▼───────────┐
                   │    ATA PIO Driver   │  ← existing (drivers/ata.c)
                   │  (0x1F0, LBA28)     │
                   └────────┬───────────┘
                            │
                   ┌────────▼───────────┐
                   │   QEMU Disk Image   │  ← host file (disk.img)
                   │   persistent on     │
                   │   host filesystem   │
                   └────────────────────┘
```

SimpleFS code is unchanged. Only the backing device changes from RAM to ATA.

### QEMU Disk Image

QEMU supports raw disk images that map 1:1 to LBA sectors:

```
Host file: disk.img (e.g. 4 MB = 8192 sectors)

Sector 0:          SimpleFS superblock
Sector 1-2:        Directory entries (32 max)
Sector 3+:         Data blocks (file contents)
```

Created with: `qemu-img create -f raw disk.img 4M`

Attached to QEMU with: `-hda disk.img` (primary master) or `-hdb disk.img` (primary slave)

The kernel boots, `ata_init()` detects the disk via IDENTIFY DEVICE, and the ATA block device adapter exposes it as a `block_device_t`.

### Reading Back From the Host

Since the disk image is raw sectors, the host can directly inspect filesystem contents:

```bash
# Read superblock (sector 0)
dd if=disk.img bs=512 count=1 | hexdump -C

# Read directory blocks (sectors 1-2)
dd if=disk.img bs=512 skip=1 count=2 | hexdump -C

# Read first data block (sector 3)
dd if=disk.img bs=512 skip=3 count=1 | strings
```

This makes it an excellent debug tool — kernel logs written to a file inside the guest OS are immediately readable from the host.

---

## Implementation Plan

### Step 1: ATA Block Device Adapter

**New file:** `drivers/ata_blockdev.c` / `drivers/ata_blockdev.h`

A minimal adapter implementing `block_device_t` over the existing ATA PIO driver:

```c
// ata_blockdev.c
#include "ata_blockdev.h"
#include "ata.h"

static block_device_t ata_dev;

static int ata_block_read(uint32_t block, void* buf) {
    return ata_read_sectors(block, 1, buf);
}

static int ata_block_write(uint32_t block, const void* buf) {
    return ata_write_sectors(block, 1, buf);
}

block_device_t* ata_blockdev_init(void) {
    const ata_drive_info_t* info = ata_get_info();
    if (!info || !info->present) return (block_device_t*)0;

    ata_dev.read = ata_block_read;
    ata_dev.write = ata_block_write;
    ata_dev.block_size = ATA_SECTOR_SIZE;
    ata_dev.total_blocks = info->lba28_sectors;

    return &ata_dev;
}
```

This is the only new code needed to make SimpleFS persistent.

### Step 2: Disk Image Setup

Add a QEMU disk image target to the Makefile:

```makefile
DISK_IMG = bin/disk.img
DISK_SIZE = 4M

$(DISK_IMG):
	qemu-img create -f raw $(DISK_IMG) $(DISK_SIZE)
```

Update the QEMU run target to attach the disk:

```makefile
run: ... $(DISK_IMG)
	qemu-system-i386 ... -hda $(DISK_IMG)
```

On first boot, `fs_init()` finds no valid superblock magic → formats the disk automatically. On subsequent boots, it finds the magic and mounts the existing filesystem.

### Step 3: Switch `kernel_main` to ATA-backed FS

**File:** `kernel/sys/kernel_main.c`

Change the filesystem initialization from RAM disk to ATA:

```c
// Before (Phase 11):
block_device_t* blk = ramdisk_init(256);
fs_init(blk);

// After (Phase 14):
block_device_t* blk = ata_blockdev_init();
if (!blk) {
    klog_warn("ATA disk not found, falling back to ramdisk");
    blk = ramdisk_init(256);
}
fs_init(blk);
```

Graceful fallback: if no ATA disk is detected (e.g. running without `-hda`), the system still works with a RAM disk.

### Step 4: Kernel Log Flush to File

**File:** `kernel/sys/klog.c`

Add a function to dump the in-memory log ring buffer to a file on the filesystem:

```c
/**
 * klog_flush_to_file - Write all buffered log entries to a file
 * @filename: Target filename (e.g. "klog.txt")
 *
 * Formats each entry as: "[LEVEL] <timestamp> <message>\n"
 * Overwrites the file contents each time.
 *
 * Returns: 0 on success, -1 on error
 */
int klog_flush_to_file(const char* filename);
```

This enables a workflow:
1. Kernel runs, generates log entries via `klog_info()`, `klog_error()`, etc.
2. Periodically (or on command), `klog_flush_to_file("klog.txt")` writes them to disk
3. From the shell: `cat klog.txt` shows the log
4. From the host: `dd if=bin/disk.img ... | strings` extracts the log without booting

### Step 5: Shell Integration

**File:** `shell/shell.c`

Add a `dmesg` command that flushes and displays kernel logs:

```
> dmesg          — display klog buffer (existing klog_dump)
> dmesg save     — flush klog to klog.txt on disk
```

This gives interactive access to debug information.

### Step 6: Dual-Device Support (Optional)

Keep the RAM disk available for scratch/temp purposes while the ATA disk serves as persistent storage. This could be useful for:
- Temp files that don't need persistence
- Test isolation (tests can use RAM disk without polluting the real FS)

---

## Disk Layout

```
ATA Disk (4 MB = 8192 sectors × 512 bytes)
─────────────────────────────────────────────
Sector 0:           Superblock
                    magic=0x41504653 ("APFS")
                    version=1
                    total_blocks=8192
                    dir_start_block=1
                    dir_block_count=2
                    data_start_block=3
                    free_blocks=8189

Sector 1-2:         Directory entries (up to 32)
                    Each entry: 32 bytes
                    { name[24], type, reserved, start_block, size }

Sector 3-8191:      Data blocks (file contents)
                    Contiguous allocation per file
─────────────────────────────────────────────
```

The layout is identical to the RAM disk — SimpleFS doesn't know or care which device it's using.

---

## What Changes and Why

### New Files

| File | Purpose |
|------|---------|
| `drivers/ata_blockdev.c` | `block_device_t` adapter over ATA PIO driver |
| `drivers/ata_blockdev.h` | Public API: `ata_blockdev_init()` |

### Modified Files

| File | Change | Reason |
|------|--------|--------|
| `kernel/sys/kernel_main.c` | Use `ata_blockdev_init()` with ramdisk fallback | Switch to persistent storage |
| `kernel/sys/klog.c` | Add `klog_flush_to_file()` | Write logs to persistent FS |
| `kernel/sys/klog.h` | Declare `klog_flush_to_file()` | Public API |
| `shell/shell.c` | Add `dmesg` / `dmesg save` command | Shell access to logs |
| `makefile` | Add disk image target, `-hda` flag, new source file | Build integration |

### Unchanged Files

| File | Why unchanged |
|------|---------------|
| `fs/simplefs.c` | Talks only to `block_device_t` — backend is transparent |
| `fs/simplefs.h` | No API changes |
| `fs/block.h` | Interface already supports this |
| `drivers/ata.c` | Already has `ata_read_sectors()` / `ata_write_sectors()` |
| `fs/ramdisk.c` | Kept as fallback; unchanged |

---

## Risks and Considerations

### First-Boot Format
On the very first boot with a fresh disk image, `fs_init()` won't find the magic number and will auto-format. This is the existing behavior — it just now applies to persistent media. Subsequent boots find the magic and mount normally.

### ATA PIO Performance
PIO mode is slow (CPU busy-waits during each 512-byte transfer). For the small files and low I/O rates of a hobby OS, this is fine. DMA would be a future optimization.

### Disk Image Corruption
If the OS crashes mid-write, the raw disk image may be left in an inconsistent state. There's no journal or fsck. For debug logging this is acceptable — a corrupt log file doesn't break the system. A future phase could add a write-ahead log.

### QEMU Disk Detection
`ata_init()` sends IDENTIFY DEVICE. If QEMU is launched without `-hda`, the drive reports as absent and the code falls back to RAM disk gracefully.

### Block Count Mismatch
The ATA IDENTIFY command returns `lba28_sectors` which may be much larger than the disk image (e.g. QEMU might report a default geometry). We use `total_blocks` from the adapter which is set to the IDENTIFY-reported size. SimpleFS only touches blocks within `superblock.total_blocks` which is set at format time, so extra sectors are harmless.

### Serial Port Alternative
For purely debug logging (no persistence needed), QEMU's `-serial file:serial.log` flag redirects COM1 output to a host file. The existing `serial_puts()` driver already works for this. The ATA-backed FS approach is better when you want logs viewable from *within* the OS shell.

---

## Debug Workflow

### From Inside the OS

```
> dmesg              ← show kernel log buffer on screen
> dmesg save         ← flush buffer to klog.txt on ATA disk
> cat klog.txt       ← read it back
> ls                 ← see klog.txt in file listing
```

### From the Host

```bash
# After QEMU exits (or while paused):
# Read raw sectors from the disk image
dd if=bin/disk.img bs=512 skip=3 count=16 2>/dev/null | strings

# Or use a small Python script to parse the SimpleFS directory
# and extract files by name (future tool)
```

### Via Serial (zero FS overhead)

```bash
# Launch QEMU with serial redirect:
qemu-system-i386 -hda bin/disk.img -serial file:serial.log ...

# All klog messages sent to serial appear in serial.log in real time
tail -f serial.log
```

---

## Success Criteria

1. QEMU launches with `-hda bin/disk.img` and the ATA driver detects the disk
2. On first boot, SimpleFS formats the disk (superblock written to sector 0)
3. Files created via `write` command persist across QEMU restarts
4. `dmesg save` writes kernel log to `klog.txt`; `cat klog.txt` shows it
5. Rebooting and running `cat klog.txt` still shows the previously saved log
6. Without `-hda`, the system falls back to RAM disk and works normally
7. The host can read `klog.txt` contents from the raw disk image via `dd | strings`
8. All existing filesystem tests (`test_fs`) pass on both ATA and RAM disk backends

---

## Future Enhancements (Not This Phase)

- **Block allocation bitmap** — Replace the scan-all-entries approach with a dedicated bitmap for O(1) allocation
- **Inode-based directory tree** — Separate metadata from names; real hierarchical directories
- **Indirect block pointers** — Support larger files beyond contiguous allocation
- **Write-ahead journal** — Crash consistency for reliable persistent storage
- **Host-side FS tool** — Python script to mount/extract files from the raw disk image
