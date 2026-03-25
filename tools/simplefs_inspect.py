#!/usr/bin/env python3
"""
simplefs_inspect.py - Inspect a raw SimpleFS disk image from the host.

Usage:
    python3 simplefs_inspect.py <disk.img>

Parses the superblock, directory entries, and optionally hexdumps
file data blocks.  Matches the on-disk layout defined in fs/simplefs.h.
"""

import sys
import struct
import os

# ── SimpleFS constants (must match fs/simplefs.h) ──
FS_MAGIC         = 0x41504653   # "APFS" little-endian
BLOCK_SIZE       = 512
ENTRY_SIZE       = 32
ENTRIES_PER_BLK  = BLOCK_SIZE // ENTRY_SIZE  # 16

TYPE_FREE = 0x00
TYPE_FILE = 0x01
TYPE_DIR  = 0x02

TYPE_NAMES = {TYPE_FREE: "FREE", TYPE_FILE: "FILE", TYPE_DIR: "DIR "}

# ── ANSI colors ──
RST = "\033[0m"
BLD = "\033[1m"
DIM = "\033[0;37m"
CYN = "\033[1;36m"
GRN = "\033[1;32m"
YLW = "\033[1;33m"
RED = "\033[1;31m"


def read_block(data, block_num):
    off = block_num * BLOCK_SIZE
    return data[off : off + BLOCK_SIZE]


def parse_superblock(blk):
    # First 32 bytes of superblock
    fields = struct.unpack_from("<IIIIIIII", blk, 0)
    return {
        "magic":            fields[0],
        "version":          fields[1],
        "total_blocks":     fields[2],
        "free_blocks":      fields[3],
        "dir_start_block":  fields[4],
        "dir_block_count":  fields[5],
        "data_start_block": fields[6],
        "first_free_block": fields[7],
    }


def parse_entry(raw):
    # 32 bytes: name[24] type(1) reserved(1) start_block(2) size(4)
    name_bytes = raw[0:24]
    etype      = raw[24]
    start_blk  = struct.unpack_from("<H", raw, 26)[0]
    size       = struct.unpack_from("<I", raw, 28)[0]
    # name is null-terminated
    name = name_bytes.split(b'\x00', 1)[0].decode('ascii', errors='replace')
    return {"name": name, "type": etype, "start_block": start_blk, "size": size}


def hexdump(data, offset=0, width=16):
    lines = []
    for i in range(0, len(data), width):
        chunk = data[i : i + width]
        hexpart = " ".join(f"{b:02x}" for b in chunk)
        ascpart = "".join(chr(b) if 0x20 <= b < 0x7f else "." for b in chunk)
        lines.append(f"  {DIM}{offset + i:08x}{RST}  {hexpart:<{width*3}}  |{ascpart}|")
    return "\n".join(lines)


def main():
    if len(sys.argv) < 2:
        print(f"Usage: {sys.argv[0]} <disk.img>")
        sys.exit(1)

    path = sys.argv[1]
    if not os.path.isfile(path):
        print(f"{RED}Error:{RST} File not found: {path}")
        sys.exit(1)

    with open(path, "rb") as f:
        data = f.read()

    img_size = len(data)
    total_sectors = img_size // BLOCK_SIZE

    # ── Banner ──
    print(f"\n{BLD}  ApPa OS{RST}{DIM}  ──  SimpleFS Image Inspector{RST}")
    print(f"{DIM}  ─────────────────────────────────────────{RST}")
    print(f"  {DIM}Image:{RST}  {path}  ({img_size} bytes, {total_sectors} sectors)")

    # ── Superblock ──
    sb_blk = read_block(data, 0)
    sb = parse_superblock(sb_blk)

    if sb["magic"] != FS_MAGIC:
        print(f"\n  {RED}Not a valid SimpleFS image{RST}  (magic: 0x{sb['magic']:08X}, expected 0x{FS_MAGIC:08X})")
        print(f"\n  {BLD}Raw Superblock Hexdump{RST}")
        print(hexdump(sb_blk[:64]))
        print()
        sys.exit(1)

    print(f"\n  {BLD}Superblock (sector 0){RST}")
    print(f"  {DIM}{'Magic:':<22}{RST} 0x{sb['magic']:08X}  {GRN}(valid){RST}")
    print(f"  {DIM}{'Version:':<22}{RST} {sb['version']}")
    print(f"  {DIM}{'Total blocks:':<22}{RST} {sb['total_blocks']}  ({sb['total_blocks'] * BLOCK_SIZE // 1024} KB)")
    print(f"  {DIM}{'Free blocks:':<22}{RST} {sb['free_blocks']}  ({sb['free_blocks'] * BLOCK_SIZE // 1024} KB)")
    used = sb["total_blocks"] - sb["free_blocks"] - sb["data_start_block"]
    print(f"  {DIM}{'Used data blocks:':<22}{RST} {used}")
    print(f"  {DIM}{'Dir start block:':<22}{RST} {sb['dir_start_block']}")
    print(f"  {DIM}{'Dir block count:':<22}{RST} {sb['dir_block_count']}")
    print(f"  {DIM}{'Data start block:':<22}{RST} {sb['data_start_block']}")

    # ── Directory entries ──
    entries = []
    for b in range(sb["dir_start_block"], sb["dir_start_block"] + sb["dir_block_count"]):
        blk = read_block(data, b)
        for i in range(ENTRIES_PER_BLK):
            raw = blk[i * ENTRY_SIZE : (i + 1) * ENTRY_SIZE]
            e = parse_entry(raw)
            if e["type"] != TYPE_FREE:
                e["_slot"] = (b - sb["dir_start_block"]) * ENTRIES_PER_BLK + i
                entries.append(e)

    print(f"\n  {BLD}Directory ({len(entries)} entries, {ENTRIES_PER_BLK * sb['dir_block_count']} max){RST}")

    if not entries:
        print(f"  {DIM}(empty){RST}")
    else:
        print(f"  {DIM}{'Slot':<6} {'Type':<6} {'Size':>8}   {'Blocks':>8}   Name{RST}")
        print(f"  {DIM}{'─'*6} {'─'*6} {'─'*8}   {'─'*8}   {'─'*23}{RST}")
        for e in entries:
            tname = TYPE_NAMES.get(e["type"], f"0x{e['type']:02X}")
            blocks_used = (e["size"] + BLOCK_SIZE - 1) // BLOCK_SIZE if e["size"] > 0 else 0
            blk_range = ""
            if e["type"] == TYPE_FILE and e["size"] > 0:
                abs_start = sb["data_start_block"] + e["start_block"]
                abs_end = abs_start + blocks_used - 1
                blk_range = f"[{abs_start}-{abs_end}]"
            print(f"  {e['_slot']:<6} {tname:<6} {e['size']:>8} B {blk_range:>10}   {e['name']}")

    # ── File contents hexdump ──
    file_entries = [e for e in entries if e["type"] == TYPE_FILE and e["size"] > 0]
    if file_entries:
        print(f"\n  {BLD}File Contents{RST}")
        for e in file_entries:
            blocks_used = (e["size"] + BLOCK_SIZE - 1) // BLOCK_SIZE
            abs_start = sb["data_start_block"] + e["start_block"]
            print(f"\n  {CYN}── {e['name']}{RST}  ({e['size']} bytes, sectors {abs_start}-{abs_start + blocks_used - 1})")

            # Read file data
            file_data = b""
            for bi in range(blocks_used):
                file_data += read_block(data, abs_start + bi)
            file_data = file_data[: e["size"]]  # trim to actual size

            # Try to show as text if printable, otherwise hexdump
            try:
                text = file_data.decode('ascii')
                if all(c == '\n' or c == '\r' or c == '\t' or (0x20 <= ord(c) < 0x7f) for c in text):
                    for line in text.split('\n'):
                        print(f"  {DIM}│{RST} {line}")
                else:
                    raise ValueError
            except (UnicodeDecodeError, ValueError):
                print(hexdump(file_data, offset=abs_start * BLOCK_SIZE))

    # ── Raw directory hexdump ──
    print(f"\n  {BLD}Raw Directory Hexdump (sectors {sb['dir_start_block']}-{sb['dir_start_block'] + sb['dir_block_count'] - 1}){RST}")
    for b in range(sb["dir_start_block"], sb["dir_start_block"] + sb["dir_block_count"]):
        blk = read_block(data, b)
        # Only show non-zero portions
        if blk == b'\x00' * BLOCK_SIZE:
            print(f"  {DIM}sector {b}: (all zeros){RST}")
        else:
            print(f"  {DIM}sector {b}:{RST}")
            # Find last non-zero byte
            last_nz = BLOCK_SIZE
            while last_nz > 0 and blk[last_nz - 1] == 0:
                last_nz -= 1
            last_nz = ((last_nz + 15) // 16) * 16  # round up to 16
            print(hexdump(blk[:last_nz], offset=b * BLOCK_SIZE))

    print()


if __name__ == "__main__":
    main()
