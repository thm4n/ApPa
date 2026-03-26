/**
 * 
 * ata.c - ATA PIO Polling Driver (Primary Master)
 * 
 * Implements LBA28 sector read/write using Programmed I/O mode.
 * The CPU polls the status register and transfers data word-by-word
 * through the ATA data port (0x1F0).
 */

#include "ata.h"
#include "ports.h"
#include "screen.h"
#include "../klibc/string.h"

/* Static drive info populated by ata_init / ata_identify */
static ata_drive_info_t drive_info;
static ata_drive_info_t slave_info;

/* ========== Internal Helpers ========== */

/**
 * ata_400ns_delay - Wait ~400ns by reading alternate status port
 * 
 * ATA spec requires a 400ns delay after sending a command before
 * the first status register read is valid.
 */
static void ata_400ns_delay(void) {
    port_byte_in(ATA_PRIMARY_ALT_STATUS);
    port_byte_in(ATA_PRIMARY_ALT_STATUS);
    port_byte_in(ATA_PRIMARY_ALT_STATUS);
    port_byte_in(ATA_PRIMARY_ALT_STATUS);
}

/**
 * ata_poll - Wait for BSY=0 and DRQ=1, checking for errors
 * 
 * Returns: 0 on success (DRQ set), -1 on error (ERR or DF set)
 */
static int ata_poll(void) {
    /* Initial 400ns delay */
    ata_400ns_delay();

    /* Wait for BSY to clear */
    while (port_byte_in(ATA_PRIMARY_STATUS) & ATA_SR_BSY)
        ;

    /* Check for errors */
    uint8_t status = port_byte_in(ATA_PRIMARY_STATUS);
    if (status & ATA_SR_ERR) return -1;
    if (status & ATA_SR_DF)  return -1;

    /* Wait for DRQ */
    if (!(status & ATA_SR_DRQ)) {
        /* DRQ not set yet, keep polling */
        while (1) {
            status = port_byte_in(ATA_PRIMARY_STATUS);
            if (status & ATA_SR_ERR) return -1;
            if (status & ATA_SR_DF)  return -1;
            if (status & ATA_SR_DRQ) break;
        }
    }

    return 0;
}

/**
 * ata_wait_ready - Wait for drive to be ready (BSY=0, DRDY=1)
 */
static void ata_wait_ready(void) {
    while (port_byte_in(ATA_PRIMARY_STATUS) & ATA_SR_BSY)
        ;
}

/**
 * ata_fix_identify_string - Fix byte-swapped ATA IDENTIFY strings
 * 
 * ATA returns model/serial strings with each pair of bytes swapped.
 * This function swaps them back and trims trailing spaces.
 */
static void ata_fix_identify_string(char* str, int len) {
    /* Swap pairs */
    for (int i = 0; i < len; i += 2) {
        char tmp = str[i];
        str[i] = str[i + 1];
        str[i + 1] = tmp;
    }

    /* Trim trailing spaces */
    str[len] = '\0';
    int end = len - 1;
    while (end >= 0 && str[end] == ' ') {
        str[end] = '\0';
        end--;
    }
}

/* ========== Internal: Parameterized IDENTIFY ========== */

/**
 * ata_identify_drive - Run IDENTIFY DEVICE on a specific drive
 * @drive_select: ATA_DRIVE_MASTER or ATA_DRIVE_SLAVE
 * @info:         Output drive info struct (zeroed on failure)
 */
static void ata_identify_drive(uint8_t drive_select, ata_drive_info_t* info) {
    memset(info, 0, sizeof(*info));

    /* Select drive */
    port_byte_out(ATA_PRIMARY_DRIVE_HEAD, drive_select);
    ata_400ns_delay();

    /* Zero out sector count and LBA registers */
    port_byte_out(ATA_PRIMARY_SECCOUNT, 0);
    port_byte_out(ATA_PRIMARY_LBA_LO, 0);
    port_byte_out(ATA_PRIMARY_LBA_MID, 0);
    port_byte_out(ATA_PRIMARY_LBA_HI, 0);

    /* Send IDENTIFY command */
    port_byte_out(ATA_PRIMARY_STATUS, ATA_CMD_IDENTIFY);
    ata_400ns_delay();

    /* Check if drive exists */
    uint8_t status = port_byte_in(ATA_PRIMARY_STATUS);
    if (status == 0) {
        info->present = 0;
        return;
    }

    /* Wait for BSY to clear */
    while ((status = port_byte_in(ATA_PRIMARY_STATUS)) & ATA_SR_BSY)
        ;

    /* Check LBA_MID and LBA_HI — if non-zero, it's not ATA */
    if (port_byte_in(ATA_PRIMARY_LBA_MID) != 0 ||
        port_byte_in(ATA_PRIMARY_LBA_HI) != 0) {
        info->present = 0;
        return;
    }

    /* Wait for DRQ or ERR */
    while (1) {
        status = port_byte_in(ATA_PRIMARY_STATUS);
        if (status & ATA_SR_ERR) {
            info->present = 0;
            return;
        }
        if (status & ATA_SR_DRQ)
            break;
    }

    /* Read 256 words of IDENTIFY data */
    uint16_t identify_data[256];
    port_words_in(ATA_PRIMARY_DATA, identify_data, 256);

    /* Extract model string (words 27-46 = 40 bytes) */
    memcpy(info->model, &identify_data[27], 40);
    ata_fix_identify_string(info->model, 40);

    /* Extract total LBA28 sectors (words 60-61) */
    info->lba28_sectors = (uint32_t)identify_data[61] << 16 |
                          (uint32_t)identify_data[60];

    /* Calculate size in MB */
    info->size_mb = info->lba28_sectors / 2048;

    /* Check LBA support (word 49, bit 9) */
    info->lba_supported = (identify_data[49] & (1 << 9)) ? 1 : 0;

    info->present = 1;
}

/* ========== Internal: Parameterized Read/Write ========== */

static int ata_read_sectors_internal(uint8_t drive_select,
                                     const ata_drive_info_t* info,
                                     uint32_t lba, uint8_t count, void* buf) {
    if (!info->present) return -1;

    uint16_t* wbuf = (uint16_t*)buf;

    ata_wait_ready();

    port_byte_out(ATA_PRIMARY_DRIVE_HEAD,
                  drive_select | ((lba >> 24) & 0x0F));

    port_byte_out(ATA_PRIMARY_SECCOUNT, count);
    port_byte_out(ATA_PRIMARY_LBA_LO,  (uint8_t)(lba & 0xFF));
    port_byte_out(ATA_PRIMARY_LBA_MID, (uint8_t)((lba >> 8) & 0xFF));
    port_byte_out(ATA_PRIMARY_LBA_HI,  (uint8_t)((lba >> 16) & 0xFF));

    port_byte_out(ATA_PRIMARY_STATUS, ATA_CMD_READ_SECTORS);

    uint8_t sectors = count == 0 ? 255 : count;
    for (uint8_t i = 0; i < sectors; i++) {
        if (ata_poll() != 0) return -1;
        port_words_in(ATA_PRIMARY_DATA, wbuf, 256);
        wbuf += 256;
    }

    return 0;
}

static int ata_write_sectors_internal(uint8_t drive_select,
                                      const ata_drive_info_t* info,
                                      uint32_t lba, uint8_t count,
                                      const void* buf) {
    if (!info->present) return -1;

    const uint16_t* wbuf = (const uint16_t*)buf;

    ata_wait_ready();

    port_byte_out(ATA_PRIMARY_DRIVE_HEAD,
                  drive_select | ((lba >> 24) & 0x0F));

    port_byte_out(ATA_PRIMARY_SECCOUNT, count);
    port_byte_out(ATA_PRIMARY_LBA_LO,  (uint8_t)(lba & 0xFF));
    port_byte_out(ATA_PRIMARY_LBA_MID, (uint8_t)((lba >> 8) & 0xFF));
    port_byte_out(ATA_PRIMARY_LBA_HI,  (uint8_t)((lba >> 16) & 0xFF));

    port_byte_out(ATA_PRIMARY_STATUS, ATA_CMD_WRITE_SECTORS);

    uint8_t sectors = count == 0 ? 255 : count;
    for (uint8_t i = 0; i < sectors; i++) {
        if (ata_poll() != 0) return -1;
        port_words_out(ATA_PRIMARY_DATA, wbuf, 256);
        wbuf += 256;
    }

    port_byte_out(ATA_PRIMARY_STATUS, ATA_CMD_CACHE_FLUSH);
    ata_wait_ready();

    return 0;
}

/* ========== Public API ========== */

void ata_init(void) {
    /* Probe primary master */
    ata_identify_drive(ATA_DRIVE_MASTER, &drive_info);

    /* Probe primary slave */
    ata_identify_drive(ATA_DRIVE_SLAVE, &slave_info);
}

int ata_read_sectors(uint32_t lba, uint8_t count, void* buf) {
    return ata_read_sectors_internal(ATA_DRIVE_MASTER, &drive_info,
                                    lba, count, buf);
}

int ata_write_sectors(uint32_t lba, uint8_t count, const void* buf) {
    return ata_write_sectors_internal(ATA_DRIVE_MASTER, &drive_info,
                                     lba, count, buf);
}

int ata_slave_read_sectors(uint32_t lba, uint8_t count, void* buf) {
    return ata_read_sectors_internal(ATA_DRIVE_SLAVE, &slave_info,
                                    lba, count, buf);
}

int ata_slave_write_sectors(uint32_t lba, uint8_t count, const void* buf) {
    return ata_write_sectors_internal(ATA_DRIVE_SLAVE, &slave_info,
                                     lba, count, buf);
}

const ata_drive_info_t* ata_get_info(void) {
    return &drive_info;
}

const ata_drive_info_t* ata_get_slave_info(void) {
    return &slave_info;
}

static void ata_print_drive(const char* label, const ata_drive_info_t* info) {
    kprint((char*)label);
    if (!info->present) {
        kprint("Not detected\n");
        return;
    }
    kprint("\n");
    kprint("  Model:    ");
    kprint((char*)info->model);
    kprint("\n");
    kprint("  Sectors:  ");
    kprint_uint(info->lba28_sectors);
    kprint("\n");
    kprint("  Size:     ");
    kprint_uint(info->size_mb);
    kprint(" MB\n");
    kprint("  LBA:      ");
    kprint(info->lba_supported ? "Supported" : "Not supported");
    kprint("\n");
}

void ata_status(void) {
    ata_print_drive("ATA Primary Master: ", &drive_info);
    ata_print_drive("ATA Primary Slave:  ", &slave_info);
}
