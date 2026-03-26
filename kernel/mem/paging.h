#ifndef PAGING_H
#define PAGING_H

#include "../../klibc/stdint.h"
#include "../arch/isr.h"

/**
 * Paging / Virtual Memory Manager
 *
 * Two-level page table structure for x86 (4 KB pages):
 *   - Page Directory: 1024 entries, each pointing to a Page Table
 *   - Page Table: 1024 entries, each pointing to a 4 KB physical frame
 *   - Total addressable: 1024 * 1024 * 4 KB = 4 GB
 */

// ─── Constants ──────────────────────────────────────────────────────────────

#define PAGES_PER_TABLE     1024
#define TABLES_PER_DIR      1024

// Page entry flags (bits 0-11 of PDE/PTE)
#define PAGE_PRESENT        0x1     // Page is present in memory
#define PAGE_WRITABLE       0x2     // Page is writable (otherwise read-only)
#define PAGE_USER           0x4     // Page accessible from Ring 3

// Mask to extract the 20-bit frame address from a PDE/PTE
#define PAGE_FRAME_MASK     0xFFFFF000

// ─── Per-Process Virtual Address Layout ────────────────────────────────────

// User code is still linked in the kernel binary (identity-mapped below 4 MB).
// The user stack gets a fixed high virtual address per process.
#define USER_STACK_VIRT     0xBFFFF000  // Virtual address for the user stack page
#define USER_STACK_TOP      0xC0000000  // ESP starts here (stack grows down)

// Number of kernel PDEs (0-3, covering 0-16 MB) that are shared/cloned
#define KERNEL_PDE_COUNT    4

// ─── Virtual Address Decomposition ─────────────────────────────────────────

// Extract PD index (bits 31-22) from virtual address
#define PAGE_DIR_INDEX(va)    (((va) >> 22) & 0x3FF)

// Extract PT index (bits 21-12) from virtual address
#define PAGE_TABLE_INDEX(va)  (((va) >> 12) & 0x3FF)

// Extract 20-bit frame base from a page entry
#define PAGE_FRAME(entry)     ((entry) & PAGE_FRAME_MASK)

// ─── Data Types ────────────────────────────────────────────────────────────

typedef uint32_t page_directory_entry_t;    // PDE: top 20 bits = PT frame, low 12 = flags
typedef uint32_t page_table_entry_t;        // PTE: top 20 bits = page frame, low 12 = flags

/**
 * page_table_t - A single page table (covers 4 MB of virtual space)
 *
 * Must be 4096-byte aligned because the PDE stores only the top 20 bits
 * of the page table's physical address (low 12 bits are flags).
 */
typedef struct {
    page_table_entry_t entries[PAGES_PER_TABLE];
} __attribute__((aligned(4096))) page_table_t;

/**
 * page_directory_t - The top-level page directory
 *
 * Only the 'entries' array is read by hardware (via CR3). It must live
 * in a page-aligned 4 KB page. The 'tables' and 'physical_address'
 * fields are software-only bookkeeping stored separately in BSS so we
 * don't overflow the single allocated page.
 */
typedef struct {
    page_directory_entry_t entries[TABLES_PER_DIR];  // Hardware-read array (4 KB)
} __attribute__((aligned(4096))) page_directory_t;

// ─── API ───────────────────────────────────────────────────────────────────

/**
 * paging_init - Set up identity mapping for 0-16 MB and enable paging
 *
 * Allocates a page directory and 4 page tables from the PMM,
 * fills them with identity mappings, registers the page fault handler
 * on ISR 14, loads CR3, and sets CR0.PG.
 *
 * Must be called after pmm_init().
 */
void paging_init(void);

/**
 * page_fault_handler - ISR 14 handler
 * @regs: CPU state at time of fault
 *
 * Reads CR2 (faulting address), decodes the error code,
 * prints a diagnostic, and halts.
 */
void page_fault_handler(registers_t* regs);

/**
 * paging_map_page - Map a single 4 KB virtual page to a physical frame
 * @virt:  Virtual address (will be page-aligned downward)
 * @phys:  Physical frame address (must be page-aligned)
 * @flags: PAGE_PRESENT, PAGE_WRITABLE, PAGE_USER OR'd together
 *
 * If the page table for this region doesn't exist yet, one is allocated
 * from the PMM automatically. Issues invlpg to flush the TLB entry.
 */
void paging_map_page(uint32_t virt, uint32_t phys, uint32_t flags);

/**
 * paging_unmap_page - Remove the mapping for a virtual page
 * @virt: Virtual address to unmap
 *
 * Clears the PTE and flushes the TLB entry. Does NOT free the
 * underlying physical frame — that is the caller's responsibility.
 */
void paging_unmap_page(uint32_t virt);

/**
 * paging_translate - Walk the page tables and return the physical address
 * @virt: Virtual address to translate
 *
 * Returns: Physical address if mapped, or 0 if the page is not present
 *          at either the PD or PT level.
 */
uint32_t paging_translate(uint32_t virt);

/**
 * paging_status - Print page directory summary
 *
 * Iterates all 1024 PD entries, counts present tables and mapped pages,
 * and prints a summary. Used by the 'pagedir' shell command.
 */
void paging_status(void);

/**
 * paging_map_user - Map a page accessible from Ring 3
 * @virt:  Virtual address (page-aligned)
 * @phys:  Physical address (page-aligned)
 * @rw:    1 = writable, 0 = read-only
 *
 * Convenience wrapper that sets PAGE_PRESENT | PAGE_USER and
 * optionally PAGE_WRITABLE.
 */
void paging_map_user(uint32_t virt, uint32_t phys, int rw);

/**
 * paging_enable_user_access - Mark identity-mapped pages as user-accessible
 *
 * Sets PAGE_USER on the PDE and every PTE in the first N page tables
 * that were created by paging_init().  This allows Ring 3 code to
 * execute kernel-linked functions (since user "programs" are currently
 * compiled into the kernel binary).
 *
 * SECURITY NOTE: This grants user-mode read/write access to ALL
 * identity-mapped memory including kernel data.  True isolation
 * requires per-process page directories (future phase).
 */
void paging_enable_user_access(void);

// ─── Per-Process Address Space API (Phase 15) ──────────────────────────────

/**
 * paging_clone_directory - Create a per-process page directory
 * @out_phys: Receives the physical address of the new page directory
 *
 * Allocates a new page directory, clones the kernel's PDE 0 page table
 * (private copy so per-process PAGE_USER bits can be set on user code
 * pages), shares PDE 1-(KERNEL_PDE_COUNT-1) verbatim (supervisor-only),
 * and clears all entries above the kernel range.
 *
 * Returns: Virtual pointer to the new page directory, or NULL on failure.
 */
page_directory_t* paging_clone_directory(uint32_t *out_phys);

/**
 * paging_map_page_in - Map a page in a specific page directory
 * @dir:   Target page directory (may differ from the kernel directory)
 * @virt:  Virtual address (page-aligned)
 * @phys:  Physical frame address (page-aligned)
 * @flags: PAGE_PRESENT | PAGE_WRITABLE | PAGE_USER etc.
 *
 * If the page table for this virtual region doesn't exist yet in @dir,
 * one is allocated from the PMM and installed.  Under identity mapping,
 * the page table physical address is used as a virtual pointer.
 */
void paging_map_page_in(page_directory_t *dir, uint32_t virt,
                        uint32_t phys, uint32_t flags);

/**
 * paging_free_directory - Free a per-process page directory and all its
 *                         private user pages and page tables
 * @dir: Virtual pointer to the page directory
 *
 * Walks PDE entries above the shared kernel range, frees every mapped
 * physical page and page table, and frees the directory page itself.
 * Also frees the private PDE 0 page table clone (but NOT the physical
 * frames it maps, since those belong to the kernel identity map).
 */
void paging_free_directory(page_directory_t *dir);

/**
 * paging_get_kernel_cr3 - Return the physical address of the kernel
 *                         page directory loaded at boot
 */
uint32_t paging_get_kernel_cr3(void);

#endif // PAGING_H
