/**
 * test_ata.c - ATA PIO Driver Tests
 */

#include "test_ata.h"
#include "../drivers/ata.h"
#include "../drivers/screen.h"
#include "../klibc/string.h"

void test_ata(void) {
    kprint("=== Testing ATA PIO Driver ===\n");

    const ata_drive_info_t* info = ata_get_info();

    /* Test 1: Drive detected */
    kprint("\nTest 1: Drive detection...\n");
    if (!info->present) {
        kprint("  [SKIP] No ATA drive detected (OK in some QEMU configs)\n");
        kprint("\n=== ATA Tests Complete (skipped) ===\n");
        return;
    }
    kprint("  [PASS] Drive present: ");
    kprint((char*)info->model);
    kprint("\n");

    /* Test 2: IDENTIFY data valid */
    kprint("\nTest 2: IDENTIFY data validation...\n");
    if (info->lba28_sectors == 0) {
        kprint("  [FAIL] LBA28 sector count is 0\n");
        return;
    }
    kprint("  [PASS] Sectors: ");
    kprint_uint(info->lba28_sectors);
    kprint(", Size: ");
    kprint_uint(info->size_mb);
    kprint(" MB\n");

    /* Test 3: LBA support */
    kprint("\nTest 3: LBA support...\n");
    if (info->lba_supported) {
        kprint("  [PASS] LBA addressing supported\n");
    } else {
        kprint("  [WARN] LBA not supported (CHS only)\n");
    }

    /* Test 4: Read boot sector (sector 0) and check signature */
    kprint("\nTest 4: Read boot sector (sector 0)...\n");
    uint8_t sector_buf[512];
    if (ata_read_sectors(0, 1, sector_buf) != 0) {
        kprint("  [FAIL] Could not read sector 0\n");
        return;
    }
    /* Check for 0x55AA boot signature at offset 510-511 */
    if (sector_buf[510] == 0x55 && sector_buf[511] == 0xAA) {
        kprint("  [PASS] Boot signature 0x55AA found at offset 510\n");
    } else {
        kprint("  [FAIL] Expected 0x55AA, got 0x");
        kprint_hex(sector_buf[510]);
        kprint_hex(sector_buf[511]);
        kprint("\n");
    }

    /* Test 5: Write/read round-trip on a high sector (safe area) */
    kprint("\nTest 5: Write/read round-trip...\n");
    /* Use a sector within the disk but beyond the kernel image.
     * Our disk image is small (~90 sectors), so use the last few. */
    const ata_drive_info_t* di = ata_get_info();
    uint32_t test_sector = di->lba28_sectors > 10 ? di->lba28_sectors - 2 : 0;

    /* Write a test pattern */
    uint8_t write_buf[512];
    for (int i = 0; i < 512; i++) {
        write_buf[i] = (uint8_t)(i & 0xFF);
    }

    if (ata_write_sectors(test_sector, 1, write_buf) != 0) {
        kprint("  [FAIL] Write to sector ");
        kprint_uint(test_sector);
        kprint(" failed\n");
        return;
    }

    /* Read it back */
    uint8_t read_buf[512];
    memset(read_buf, 0, 512);
    if (ata_read_sectors(test_sector, 1, read_buf) != 0) {
        kprint("  [FAIL] Read from sector ");
        kprint_uint(test_sector);
        kprint(" failed\n");
        return;
    }

    /* Compare */
    if (memcmp(write_buf, read_buf, 512) == 0) {
        kprint("  [PASS] 512-byte round-trip matches\n");
    } else {
        kprint("  [FAIL] Data mismatch after round-trip\n");
    }

    kprint("\n=== ATA Tests Complete ===\n");
}
