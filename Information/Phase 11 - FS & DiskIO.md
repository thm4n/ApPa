# Phase 11: File System & Disk I/O

## Overview
Implement an ATA PIO disk driver for raw sector access and a RAM-disk-backed simple filesystem with a block device abstraction layer. This gives the kernel persistent storage primitives and lets users create, read, write, and delete files from the shell.

**Goal:** ATA PIO polling driver, block device interface, RAM disk, custom simple filesystem, six new shell commands (`ls`, `cat`, `write`, `mkdir`, `rm`, `disk`).

**Why we need this:**
- **Storage foundation** — Every OS needs to read/write data that outlives a single function call
- **Block device abstraction** — Decouples filesystem logic from hardware, enabling seamless swap from RAM disk to ATA disk later
- **File operations** — Programs need `open/read/write/close` semantics to manage data
- **Shell usability** — Users can create and inspect files interactively
- **Forward compatibility** — The block device interface enables FAT12, ext2, or any future FS without changing the driver layer

---

## Theory

### ATA PIO (Programmed I/O)

ATA (AT Attachment) is the standard interface for IDE hard drives. PIO mode means the CPU directly transfers data between RAM and the disk controller via I/O ports — no DMA controller involved.

#### ATA Register Map (Primary Bus)

```
Port      Read                    Write
──────    ──────────────────      ──────────────────────
0x1F0     Data (16-bit)           Data (16-bit)
0x1F1     Error Register          Features
0x1F2     Sector Count            Sector Count
0x1F3     LBA Low  (bits 0-7)     LBA Low  (bits 0-7)
0x1F4     LBA Mid  (bits 8-15)    LBA Mid  (bits 8-15)
0x1F5     LBA High (bits 16-23)   LBA High (bits 16-23)
0x1F6     Drive/Head              Drive/Head
0x1F7     Status                  Command
0x3F6     Alternate Status        Device Control
```

#### LBA28 Addressing

LBA (Logical Block Addressing) maps each 512-byte sector to a sequential number:

```
Sector 0  = bytes 0–511       (boot sector)
Sector 1  = bytes 512–1023
Sector 2  = bytes 1024–1535
...
Sector N  = bytes N*512 – (N+1)*512-1
```

LBA28 uses 28 bits → max 2^28 sectors = 128 GB addressable. More than enough.

The 28 LBA bits are split across four registers:

```
0x1F3: LBA bits 0–7
0x1F4: LBA bits 8–15
0x1F5: LBA bits 16–23
0x1F6: LBA bits 24–27 (lower nibble) + drive select (bit 4) + LBA mode (bit 6)
```

#### PIO Read Sequence

```
1. Wait for BSY=0 (controller not busy)
2. Select drive: write 0xE0 | (drive << 4) | (lba >> 24 & 0x0F) to 0x1F6
3. Write sector count to 0x1F2
4. Write LBA low/mid/high to 0x1F3, 0x1F4, 0x1F5
5. Send READ SECTORS command (0x20) to 0x1F7
6. For each sector:
   a. Poll: wait for BSY=0, DRQ=1 (data ready)
   b. Read 256 words (512 bytes) from 0x1F0
   c. 400ns delay (read alternate status port 0x3F6)
```

#### PIO Write Sequence

```
1–5. Same as read, but send WRITE SECTORS command (0x30)
6. For each sector:
   a. Poll: wait for BSY=0, DRQ=1
   b. Write 256 words to 0x1F0
   c. 400ns delay
7. Send CACHE FLUSH command (0xE7) after all sectors written
```

#### IDENTIFY DEVICE

Command `0xEC` returns 256 words (512 bytes) of drive metadata:

```
Word(s)    Content
────────   ──────────────────────────────
27–46      Model string (40 ASCII chars, byte-swapped in pairs)
60–61      Total LBA28 sectors (uint32_t, little-endian)
83         Bit 10 = LBA48 supported
88         Ultra DMA modes supported
```

#### Status Register Bits (0x1F7 read)

```
Bit   Name   Meaning
───   ────   ─────────────────────────────
7     BSY    Controller busy — do NOT read other registers
6     DRDY   Drive ready
5     DF     Drive fault (unrecoverable)
4     SRV    Service request
3     DRQ    Data ready for transfer
2     CORR   Correctable error (obsolete)
1     IDX    Index mark (obsolete)
0     ERR    Error occurred — read Error register for details
```

#### Polling Algorithm

```
ata_poll():
  1. Read alternate status (0x3F6) — 400ns delay
  2. Loop:
     a. Read status (0x1F7)
     b. If BSY=1, continue loop
     c. If ERR=1 or DF=1, return error
     d. If DRQ=1, return success
     e. Continue loop
```

---

### Block Device Abstraction

A block device is an interface — anything that can read and write fixed-size blocks:

```
┌─────────────┐     ┌─────────────┐     ┌─────────────┐
│  SimpleFS    │     │  SimpleFS    │     │  FAT12       │
│  (custom)    │     │  (custom)    │     │  (future)    │
└──────┬──────┘     └──────┬──────┘     └──────┬──────┘
       │                   │                   │
       ▼                   ▼                   ▼
┌──────────────────────────────────────────────────────┐
│              Block Device Interface                   │
│     read(block_num, buf)  /  write(block_num, buf)   │
└──────┬──────────────────────────────┬────────────────┘
       │                              │
       ▼                              ▼
┌─────────────┐              ┌─────────────┐
│  RAM Disk    │              │  ATA PIO     │
│  (memcpy)    │              │  (port I/O)  │
└─────────────┘              └─────────────┘
```

The filesystem talks to the block device interface. The block device implementation can be RAM-backed (for development) or ATA-backed (for real hardware). **Swapping the backend requires zero changes to filesystem code.**

```c
typedef struct {
    int (*read)(uint32_t block, void* buf);
    int (*write)(uint32_t block, const void* buf);
    uint32_t block_size;      // 512 bytes (matches ATA sector size)
    uint32_t total_blocks;
} block_device_t;
```

---

### SimpleFS — Custom Filesystem Design

A minimal filesystem for learning. Flat directory structure (v1), fixed-size entries, contiguous file allocation.

#### Disk Layout

```
Block 0          Superblock (512 bytes)
Block 1–N        Directory entries (16 entries per block, 32 bytes each)
Block N+1–end    Data blocks (file contents)

Example: 512 blocks × 512 bytes = 256 KB filesystem
  Block 0:       Superblock
  Block 1–2:     Directory (32 entries max)
  Block 3–511:   Data blocks (509 × 512 = ~254 KB usable)
```

#### Superblock (Block 0)

```
Offset  Size     Field
──────  ──────   ─────────────────────────
0x00    4 bytes  Magic number ("APFS" = 0x41504653)
0x04    4 bytes  Version (1)
0x08    4 bytes  Total blocks
0x0C    4 bytes  Free blocks
0x10    4 bytes  Root directory start block
0x14    4 bytes  Root directory block count
0x18    4 bytes  Data region start block
0x1C    4 bytes  First free data block
0x20    472 B    Reserved (zero-filled)
```

#### Directory Entry (32 bytes)

```
Offset  Size     Field
──────  ──────   ─────────────────────────
0x00    24 bytes Name (null-terminated, max 23 chars)
0x18    1 byte   Type (0x00=free, 0x01=file, 0x02=directory)
0x19    1 byte   Reserved
0x1A    2 bytes  Start block (data region)
0x1C    4 bytes  Size in bytes (for files)
```

16 entries fit in one 512-byte block (16 × 32 = 512).

#### File Storage

Files use contiguous block allocation:
- Small file (≤512B): 1 data block
- Larger file: consecutive blocks from `start_block`
- Blocks needed = `(size + 511) / 512`

```
Directory Entry:               Data Blocks:
┌──────────────────┐          ┌────────┬────────┬────────┐
│ name: "hello.txt"│          │Block 5 │Block 6 │Block 7 │
│ type: FILE       │────────► │ data   │ data   │ data   │
│ start: 5         │          │ 512B   │ 512B   │ 200B   │
│ size: 1224       │          └────────┴────────┴────────┘
└──────────────────┘           (3 blocks for 1224 bytes)
```

---

## Memory Layout

```
                     ┌────────────────────────────────────┐
0x00000000           │ IVT + BIOS Data                    │
                     ├────────────────────────────────────┤
0x00000600           │ Stage2 bootloader (2KB, dead after  │
                     │ protected mode switch)              │
                     ├────────────────────────────────────┤
0x00001000           │ Kernel .text / .rodata / .data      │
                     │ .bss (up to ~0x13688)               │
                     ├────────────────────────────────────┤
0x0009FC00           │ Protected-mode stack (grows down)   │
                     ├────────────────────────────────────┤
0x000A0000           │ VGA memory                          │
                     ├────────────────────────────────────┤
0x000C0000           │ BIOS ROM                            │
                     ├────────────────────────────────────┤
0x00100000 (1MB)     │ Kernel heap (kmalloc, 1MB)          │
                     ├────────────────────────────────────┤
0x00200000 (2MB)     │ PMM bitmap (512 bytes)              │
                     ├────────────────────────────────────┤
0x00201000           │ PMM allocation pool (~13MB)          │
                     │  ├─ Page directory + page tables     │
                     │  ├─ RAM disk pages (allocated here)  │  ◄── NEW
                     │  └─ Other dynamic allocations        │
                     ├────────────────────────────────────┤
0x00F00000 (15MB)    │ Device / reserved memory             │
                     ├────────────────────────────────────┤
0x00FFFFFF (16MB)    │ End of managed memory                │
                     └────────────────────────────────────┘

RAM disk is backed by PMM pages from the allocation pool.
A 256KB RAM disk = 64 pages = 256KB of the ~13MB pool.
```

---

## Implementation

### Files Created

| File | Purpose |
|------|---------|
| `drivers/ata.h` | ATA port defines, status bits, IDENTIFY struct, API declarations |
| `drivers/ata.c` | ATA PIO polling driver — detect, identify, read/write sectors |
| `fs/block.h` | Block device interface (function pointer struct) |
| `fs/ramdisk.h` | RAM disk block device declarations |
| `fs/ramdisk.c` | RAM disk implementation — PMM-backed `memcpy` read/write |
| `fs/simplefs.h` | SimpleFS types (superblock, dir entry), API declarations |
| `fs/simplefs.c` | Filesystem logic — format, create, read, write, delete, list |
| `tests/test_ata.h` | ATA test declarations |
| `tests/test_ata.c` | ATA unit tests — IDENTIFY, sector read, write round-trip |
| `tests/test_fs.h` | Filesystem test declarations |
| `tests/test_fs.c` | FS unit tests — create, write, read, delete, list, edge cases |

### Files Modified

| File | Change |
|------|--------|
| `drivers/ports.h` | Add `port_words_in()`, `port_words_out()`, `io_wait()` |
| `drivers/ports.c` | Implement bulk port I/O (`rep insw` / `rep outsw`) and `io_wait()` |
| `libc/string.h` | Add `memcmp()`, `strchr()`, `strrchr()` |
| `libc/string.c` | Implement the above |
| `kernel/kernel_main.c` | Add `ata_init()`, `ramdisk_init()`, `fs_init()` to init sequence |
| `kernel/shell.c` | Add `ls`, `cat`, `write`, `mkdir`, `rm`, `disk` commands |
| `tests/tests.h` | Include `test_ata.h`, `test_fs.h` |
| `tests/tests.c` | Add `test_ata()`, `test_fs()` to runner |
| `README.md` | Mark Phase 11 completed |

### API

#### ATA Driver

```c
void     ata_init(void);                                          // Detect + IDENTIFY primary master
int      ata_read_sectors(uint32_t lba, uint8_t count, void* buf);  // PIO polling read
int      ata_write_sectors(uint32_t lba, uint8_t count, const void* buf); // PIO polling write
int      ata_identify(void);                                      // Run IDENTIFY DEVICE, store results
void     ata_status(void);                                        // Print drive info (shell: disk)
```

#### Block Device

```c
typedef struct {
    int (*read)(uint32_t block, void* buf);
    int (*write)(uint32_t block, const void* buf);
    uint32_t block_size;
    uint32_t total_blocks;
} block_device_t;
```

#### RAM Disk

```c
block_device_t* ramdisk_init(uint32_t size_kb);   // Allocate PMM pages, return block device
```

#### SimpleFS

```c
int       fs_init(block_device_t* dev);                              // Mount or format
int       fs_create(const char* name, uint8_t type);                 // Create file or dir
int       fs_write_file(const char* name, const void* data, uint32_t size); // Write file
int32_t   fs_read_file(const char* name, void* buf, uint32_t max_size);     // Read file
int       fs_delete(const char* name);                               // Delete entry + free blocks
uint32_t  fs_list(fs_entry_t* entries, uint32_t max);                // List directory entries
int       fs_stat(const char* name, fs_entry_t* out);                // Get file metadata
```

### Initialization Order

```
kernel_main.c init sequence (Phase 11 additions marked ►):

  serial_init()
  clear_screen()
  gdt_init()
  idt_init()
  pic_remap(32, 40)
  pit_init(100)
  timer_init()
  kmalloc_init()
  pmm_init()
  paging_init()
► ata_init()           // Detect ATA drive, run IDENTIFY
► ramdisk_init(256)    // Allocate 256KB RAM disk from PMM
► fs_init(&ramdisk)    // Format SimpleFS on RAM disk
  keyboard_init()
  shell_init()
  sti
  run_all_tests()
```

---

## Testing Strategy

### ATA Tests

| # | Test | What It Verifies |
|---|------|-----------------|
| 1 | IDENTIFY DEVICE | Returns valid model string and sector count |
| 2 | Read boot sector | Sector 0 contains `0x55AA` at offset 510 |
| 3 | Write/read round-trip | Write pattern to high sector, read back, compare |

### Filesystem Tests

| # | Test | What It Verifies |
|---|------|-----------------|
| 1 | Format check | Superblock has correct magic `"APFS"` after init |
| 2 | Create + list | Create a file, `fs_list()` returns it |
| 3 | Write + read | Write data, read back, `memcmp` matches |
| 4 | Delete + verify | Delete file, `fs_list()` no longer includes it |
| 5 | Multiple files | Create 3 files, list shows all 3 |
| 6 | Overwrite | Write to existing file replaces content |
| 7 | Read nonexistent | Returns error code, doesn't crash |
| 8 | Create duplicate | Returns error code for duplicate name |
| 9 | Directory entry | `mkdir` creates entry with type=DIR |
| 10 | Delete + reclaim | Delete frees data blocks, `fs_create` can reuse space |

---

## Shell Commands

```
> disk
ATA Primary Master:
  Model:    QEMU HARDDISK
  Sectors:  16384
  Size:     8 MB
  LBA:      Supported

> ls
  FILE    1224 B   hello.txt
  FILE      42 B   notes.txt
  DIR        0 B   mydir
  3 entries

> cat hello.txt
Hello, World! This is ApPa OS.

> write test.txt This is a test file
Written 23 bytes to 'test.txt'

> mkdir docs
Created directory 'docs'

> rm test.txt
Deleted 'test.txt'

> ls
  FILE    1224 B   hello.txt
  FILE      42 B   notes.txt
  DIR        0 B   mydir
  DIR        0 B   docs
  4 entries
```

---

## Common Pitfalls

| Pitfall | Explanation |
|---------|-------------|
| Not waiting for BSY=0 before reading registers | ATA spec: when BSY=1, all other status bits are undefined. Always poll BSY first. |
| Forgetting the 400ns delay | After sending a command, read alternate status (0x3F6) once before polling 0x1F7. The first status read may be stale. |
| Byte-swapped IDENTIFY strings | ATA returns model/serial strings with each pair of bytes swapped. `"QMUE"` → `"QEMU"`. |
| Reading 0x1F7 clears pending IRQ | If you later add IRQ support, be aware that status reads have side effects. Use 0x3F6 (alternate status) for polling without clearing IRQs. |
| Not sending CACHE FLUSH after write | Writes may be buffered by the controller. Send command 0xE7 after the last sector to ensure data hits the platter (or QEMU's image file). |
| Contiguous allocation fragmentation | SimpleFS v1 uses contiguous blocks. After many create/delete cycles, large files may fail to allocate even with enough total free space. Acceptable for v1. |
| Block size mismatch | RAM disk and ATA both use 512-byte blocks. Keep this consistent to make backend swapping seamless. |
| Superblock not written back | After any metadata change (create, delete), flush the superblock to block 0. Otherwise state is lost if you later persist to disk. |

---

## What This Enables Next

- **ATA-backed persistence** — Swap RAM disk backend for ATA `read_sectors`/`write_sectors` → files survive reboot
- **FAT12 support** — Implement as an alternative filesystem behind the same block device interface
- **ELF loader** — Read executable files from the filesystem, load into memory, jump to entry point
- **Shell scripting** — Read and execute command files from the filesystem
- **Logging to disk** — Persist `klog` output to a file for post-mortem debugging
- **Multi-directory support** — Extend SimpleFS with recursive directory traversal and path resolution (`/dir/subdir/file`)