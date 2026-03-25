#ifndef ATA_H
#define ATA_H

#include "../libc/stdint.h"

/**
 * ATA PIO Driver
 * 
 * Provides raw sector read/write access to ATA/IDE disks using
 * PIO (Programmed I/O) polling mode via the primary ATA bus.
 * 
 * LBA28 addressing: supports up to 128GB (2^28 sectors × 512 bytes).
 */

/* ========== Primary ATA Bus I/O Ports ========== */
#define ATA_PRIMARY_DATA        0x1F0   /* Data register (16-bit R/W) */
#define ATA_PRIMARY_ERROR       0x1F1   /* Error register (R) / Features (W) */
#define ATA_PRIMARY_SECCOUNT    0x1F2   /* Sector count */
#define ATA_PRIMARY_LBA_LO      0x1F3   /* LBA bits 0-7 */
#define ATA_PRIMARY_LBA_MID     0x1F4   /* LBA bits 8-15 */
#define ATA_PRIMARY_LBA_HI      0x1F5   /* LBA bits 16-23 */
#define ATA_PRIMARY_DRIVE_HEAD  0x1F6   /* Drive/Head + LBA bits 24-27 */
#define ATA_PRIMARY_STATUS      0x1F7   /* Status (R) / Command (W) */
#define ATA_PRIMARY_ALT_STATUS  0x3F6   /* Alternate Status (R) / Device Control (W) */

/* ========== ATA Commands ========== */
#define ATA_CMD_READ_SECTORS    0x20    /* READ SECTORS (PIO) */
#define ATA_CMD_WRITE_SECTORS   0x30    /* WRITE SECTORS (PIO) */
#define ATA_CMD_CACHE_FLUSH     0xE7    /* CACHE FLUSH */
#define ATA_CMD_IDENTIFY        0xEC    /* IDENTIFY DEVICE */

/* ========== Status Register Bits ========== */
#define ATA_SR_BSY              0x80    /* Busy */
#define ATA_SR_DRDY             0x40    /* Drive ready */
#define ATA_SR_DF               0x20    /* Drive fault */
#define ATA_SR_DRQ              0x08    /* Data request ready */
#define ATA_SR_ERR              0x01    /* Error */

/* ========== Drive Select ========== */
#define ATA_DRIVE_MASTER        0xE0    /* Master drive, LBA mode */
#define ATA_DRIVE_SLAVE         0xF0    /* Slave drive, LBA mode */

/* ========== Device Indices ========== */
#define ATA_DEV_MASTER          0
#define ATA_DEV_SLAVE           1

/* ========== Sector Size ========== */
#define ATA_SECTOR_SIZE         512

/* ========== Drive Info (from IDENTIFY) ========== */
typedef struct {
    char     model[41];         /* Model string (40 chars + null) */
    uint32_t lba28_sectors;     /* Total addressable sectors (LBA28) */
    uint32_t size_mb;           /* Drive size in MB */
    uint8_t  present;           /* 1 if drive detected, 0 if absent */
    uint8_t  lba_supported;     /* 1 if LBA addressing supported */
} ata_drive_info_t;

/**
 * ata_init - Detect and identify the primary master ATA drive
 * 
 * Sends IDENTIFY DEVICE command and stores drive metadata.
 * Safe to call if no drive is present (sets present=0).
 */
void ata_init(void);

/**
 * ata_read_sectors - Read sectors from disk using PIO polling
 * @lba:   Starting LBA sector number
 * @count: Number of sectors to read (1-255, 0 means 256)
 * @buf:   Destination buffer (must be at least count * 512 bytes)
 * 
 * Returns: 0 on success, -1 on error
 */
int ata_read_sectors(uint32_t lba, uint8_t count, void* buf);

/**
 * ata_write_sectors - Write sectors to disk using PIO polling
 * @lba:   Starting LBA sector number
 * @count: Number of sectors to write (1-255, 0 means 256)
 * @buf:   Source buffer (must be at least count * 512 bytes)
 * 
 * Returns: 0 on success, -1 on error
 */
int ata_write_sectors(uint32_t lba, uint8_t count, const void* buf);

/**
 * ata_get_info - Get master drive information from last IDENTIFY
 * 
 * Returns: Pointer to static ata_drive_info_t struct
 */
const ata_drive_info_t* ata_get_info(void);

/**
 * ata_get_slave_info - Get slave drive information from last IDENTIFY
 * 
 * Returns: Pointer to static ata_drive_info_t struct
 */
const ata_drive_info_t* ata_get_slave_info(void);

/**
 * ata_slave_read_sectors - Read sectors from slave drive using PIO
 * @lba:   Starting LBA sector number
 * @count: Number of sectors to read (1-255)
 * @buf:   Destination buffer (must be at least count * 512 bytes)
 * 
 * Returns: 0 on success, -1 on error
 */
int ata_slave_read_sectors(uint32_t lba, uint8_t count, void* buf);

/**
 * ata_slave_write_sectors - Write sectors to slave drive using PIO
 * @lba:   Starting LBA sector number
 * @count: Number of sectors to write (1-255)
 * @buf:   Source buffer (must be at least count * 512 bytes)
 * 
 * Returns: 0 on success, -1 on error
 */
int ata_slave_write_sectors(uint32_t lba, uint8_t count, const void* buf);

/**
 * ata_status - Print drive info to screen (for shell 'disk' command)
 */
void ata_status(void);

#endif /* ATA_H */
