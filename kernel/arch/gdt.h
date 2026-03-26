#ifndef GDT_H
#define GDT_H

#include "../../klibc/stdint.h"

// ─── Segment Selectors ────────────────────────────────────────────────────
// These match the GDT layout: null(0), kcode(1), kdata(2), ucode(3), udata(4), tss(5)
#define GDT_KERNEL_CODE_SEG  0x08   // GDT entry 1, RPL=0
#define GDT_KERNEL_DATA_SEG  0x10   // GDT entry 2, RPL=0
#define GDT_USER_CODE_SEG    0x1B   // GDT entry 3, RPL=3 (0x18 | 3)
#define GDT_USER_DATA_SEG    0x23   // GDT entry 4, RPL=3 (0x20 | 3)
#define GDT_TSS_SEG          0x2B   // GDT entry 5, RPL=3 (0x28 | 3)

// Total number of GDT entries
#define GDT_ENTRIES 6

// GDT entry structure
struct gdt_entry {
    uint16_t limit_low;
    uint16_t base_low;
    uint8_t  base_middle;
    uint8_t  access;
    uint8_t  granularity;
    uint8_t  base_high;
} __attribute__((packed));

// GDT pointer structure
struct gdt_ptr {
    uint16_t limit;
    uint32_t base;
} __attribute__((packed));

// Function declarations
void gdt_init(void);

/**
 * gdt_set_gate_ext - Set a GDT entry (exposed for TSS installation)
 * @num:    GDT slot index
 * @base:   Segment base address
 * @limit:  Segment limit
 * @access: Access byte
 * @gran:   Granularity byte
 */
void gdt_set_gate_ext(int32_t num, uint32_t base, uint32_t limit,
                       uint8_t access, uint8_t gran);

/**
 * gdt_reload - Reload the GDT pointer and flush segment registers
 *
 * Called after adding new entries (e.g. TSS) so the CPU picks up
 * the updated table size.
 */
void gdt_reload(void);

#endif
