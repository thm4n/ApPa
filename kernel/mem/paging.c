/**
 * paging.c - Paging / Virtual Memory Manager
 *
 * Identity-maps the first 16 MB of physical RAM using two-level 4 KB page
 * tables, enables the MMU via CR0.PG, and provides map/unmap/translate
 * primitives for future use.
 */

#include "paging.h"
#include "pmm.h"
#include "../arch/isr.h"
#include "../task/task.h"
#include "../sys/klog.h"
#include "../../drivers/screen.h"
#include "../../libc/string.h"

// ─── Globals ───────────────────────────────────────────────────────────────

// The kernel page directory (single address space for now)
// 'kernel_directory' points to the hardware PDE array (lives in a PMM page)
static page_directory_t* kernel_directory = 0;

// Software bookkeeping — stored in BSS, NOT in the PDE page
// These track virtual pointers to page tables for easy C access
static page_table_t* pd_tables[TABLES_PER_DIR];

// Physical address loaded into CR3
static uint32_t pd_physical_address = 0;

// ─── Inline Helpers ────────────────────────────────────────────────────────

/**
 * flush_tlb_entry - Invalidate a single TLB entry
 * @virt: Virtual address whose cached translation should be flushed
 */
static inline void flush_tlb_entry(uint32_t virt) {
    __asm__ volatile("invlpg (%0)" : : "r"(virt) : "memory");
}

// ─── Page Fault Handler (ISR 14) ───────────────────────────────────────────

/**
 * page_fault_handler - Called by the IDT when a page fault occurs
 *
 * Reads the faulting virtual address from CR2, decodes the error code
 * pushed by the CPU, prints a diagnostic, and halts.
 */
void page_fault_handler(registers_t* regs) {
    // Disable interrupts so timer doesn't corrupt our output
    __asm__ volatile("cli");

    // CR2 contains the virtual address that caused the fault
    uint32_t faulting_addr;
    __asm__ volatile("mov %%cr2, %0" : "=r"(faulting_addr));

    // Decode error code bits
    int present  = regs->err_code & 0x1;   // 0 = not-present, 1 = protection
    int write    = regs->err_code & 0x2;   // 0 = read, 1 = write
    int user     = regs->err_code & 0x4;   // 0 = supervisor, 1 = user
    int reserved = regs->err_code & 0x8;   // 1 = reserved bit overwritten
    int ifetch   = regs->err_code & 0x10;  // 1 = instruction fetch

    // CR2 and error code are available — build a diagnostic string
    char hex_buf[12];

    kprint("\n!!! PAGE FAULT !!!\n");
    kprint("  Faulting address: 0x");
    uitoa(faulting_addr, hex_buf, 16);
    kprint(hex_buf);
    kprint("\n");

    // Print fault type
    kprint("  Cause: ");
    if (!present) kprint("page not present");
    else          kprint("protection violation");

    if (write)    kprint(" | write");
    else          kprint(" | read");

    if (user)     kprint(" | user-mode");
    else          kprint(" | supervisor-mode");

    if (reserved) kprint(" | reserved-bit");
    if (ifetch)   kprint(" | instruction-fetch");
    kprint("\n");

    // Print EIP where the fault occurred
    kprint("  EIP: 0x");
    uitoa(regs->eip, hex_buf, 16);
    kprint(hex_buf);
    kprint("\n");

    // Print error code
    kprint("  Error code: 0x");
    uitoa(regs->err_code, hex_buf, 16);
    kprint(hex_buf);
    kprint("\n");

    /* ── Log to klog so dmesg captures it ── */
    if (user) {
        task_t *cur = task_get_current();
        klog_error("PAGE FAULT addr=0x%x eip=0x%x err=0x%x task='%s' [user] -> killed",
                   faulting_addr, regs->eip, regs->err_code,
                   cur ? cur->name : "?");
        kprint("  -> Killing user task '");
        if (cur) kprint(cur->name);
        kprint("'\n");
        task_exit();
        /* Never reached */
    }

    /* Kernel-mode fault */
    klog_error("PAGE FAULT addr=0x%x eip=0x%x err=0x%x [kernel] -> halted",
               faulting_addr, regs->eip, regs->err_code);

    // Kernel-mode fault — halt (unrecoverable)
    kprint("System halted.\n");
    __asm__ volatile("hlt");
}

// ─── Initialization ────────────────────────────────────────────────────────

/**
 * paging_init - Identity-map 0-16 MB and enable paging
 *
 * Flow:
 *   1. Allocate + zero page directory from PMM
 *   2. For each 4 MB chunk [0..16 MB): allocate + fill page table
 *   3. Register page fault handler on ISR 14
 *   4. Load CR3, set CR0.PG
 */
void paging_init(void) {
    // ── Step 1: Allocate page directory ──
    uint32_t pd_phys = alloc_page();
    if (pd_phys == 0) {
        kprint("PANIC: paging_init - cannot allocate page directory\n");
        return;
    }

    // Under identity mapping (not yet active), physical == virtual
    kernel_directory = (page_directory_t*)pd_phys;
    memset(kernel_directory->entries, 0, sizeof(kernel_directory->entries));
    memset(pd_tables, 0, sizeof(pd_tables));
    pd_physical_address = pd_phys;

    // ── Step 2: Identity-map 0 – 16 MB (4 page tables) ──
    // Each page table covers 4 MB (1024 entries × 4 KB per page)
    for (uint32_t i = 0; i < 4; i++) {
        uint32_t pt_phys = alloc_page();
        if (pt_phys == 0) {
            kprint("PANIC: paging_init - cannot allocate page table\n");
            return;
        }

        page_table_t* pt = (page_table_t*)pt_phys;

        // Fill 1024 PTEs: identity map this 4 MB chunk
        uint32_t chunk_base = i * 0x400000;  // i * 4 MB
        for (uint32_t j = 0; j < PAGES_PER_TABLE; j++) {
            uint32_t phys_addr = chunk_base + j * 0x1000;
            pt->entries[j] = phys_addr | PAGE_PRESENT | PAGE_WRITABLE;
        }

        // Install PDE
        kernel_directory->entries[i] = pt_phys | PAGE_PRESENT | PAGE_WRITABLE;
        pd_tables[i] = pt;
    }

    // ── Step 3: Register page fault handler (ISR 14) ──
    register_interrupt_handler(14, page_fault_handler);

    // ── Step 4: Load CR3 and enable paging ──
    __asm__ volatile("mov %0, %%cr3" : : "r"(pd_phys));

    uint32_t cr0;
    __asm__ volatile("mov %%cr0, %0" : "=r"(cr0));
    cr0 |= 0x80000000;  // Set PG bit (bit 31)
    __asm__ volatile("mov %0, %%cr0" : : "r"(cr0));

    // Paging is now active — identity mapping means everything still works
}

// ─── Map / Unmap / Translate ───────────────────────────────────────────────

/**
 * paging_map_page - Map a 4 KB virtual page to a physical frame
 */
void paging_map_user(uint32_t virt, uint32_t phys, int rw) {
    uint32_t flags = PAGE_PRESENT | PAGE_USER;
    if (rw) flags |= PAGE_WRITABLE;
    paging_map_page(virt, phys, flags);
}

void paging_map_page(uint32_t virt, uint32_t phys, uint32_t flags) {
    uint32_t pd_idx = PAGE_DIR_INDEX(virt);
    uint32_t pt_idx = PAGE_TABLE_INDEX(virt);

    // If no page table exists for this PD entry, allocate one
    if (!(kernel_directory->entries[pd_idx] & PAGE_PRESENT)) {
        uint32_t pt_phys = alloc_page();
        if (pt_phys == 0) {
            kprint("paging_map_page: out of memory for page table\n");
            return;
        }

        page_table_t* pt = (page_table_t*)pt_phys;
        memset(pt->entries, 0, sizeof(pt->entries));

        kernel_directory->entries[pd_idx] = pt_phys | PAGE_PRESENT | PAGE_WRITABLE | (flags & PAGE_USER);
        pd_tables[pd_idx] = pt;
    } else if (flags & PAGE_USER) {
        /* PDE already exists — ensure PAGE_USER is propagated.
         * x86 checks both the PDE and PTE User/Supervisor bits;
         * if the PDE lacks it, Ring 3 can never access the page. */
        kernel_directory->entries[pd_idx] |= PAGE_USER;
    }

    // Set the PTE
    page_table_t* table = pd_tables[pd_idx];
    table->entries[pt_idx] = (phys & PAGE_FRAME_MASK) | (flags & 0xFFF) | PAGE_PRESENT;

    // Flush TLB for this virtual address
    flush_tlb_entry(virt);
}

/**
 * paging_enable_user_access - Mark 0-16 MB identity mapping as user-accessible
 */
void paging_enable_user_access(void) {
    /* The first 4 page tables (indices 0-3) cover 0-16 MB.
     * Set PAGE_USER on each PDE and every PTE within. */
    for (uint32_t i = 0; i < 4; i++) {
        if (!(kernel_directory->entries[i] & PAGE_PRESENT))
            continue;

        /* Set USER bit on the PDE */
        kernel_directory->entries[i] |= PAGE_USER;

        /* Set USER bit on every PTE in this table */
        page_table_t *pt = pd_tables[i];
        if (!pt) continue;

        for (uint32_t j = 0; j < PAGES_PER_TABLE; j++) {
            if (pt->entries[j] & PAGE_PRESENT) {
                pt->entries[j] |= PAGE_USER;
            }
        }
    }

    /* Flush TLB by reloading CR3 */
    uint32_t cr3;
    __asm__ volatile("mov %%cr3, %0" : "=r"(cr3));
    __asm__ volatile("mov %0, %%cr3" : : "r"(cr3));
}

/**
 * paging_unmap_page - Remove mapping for a virtual page
 */
void paging_unmap_page(uint32_t virt) {
    uint32_t pd_idx = PAGE_DIR_INDEX(virt);
    uint32_t pt_idx = PAGE_TABLE_INDEX(virt);

    // Check PDE present
    if (!(kernel_directory->entries[pd_idx] & PAGE_PRESENT)) {
        return;  // No page table — nothing to unmap
    }

    page_table_t* table = pd_tables[pd_idx];
    if (!table) {
        return;
    }

    // Clear PTE
    table->entries[pt_idx] = 0;

    // Flush TLB for this virtual address
    flush_tlb_entry(virt);
}

/**
 * paging_translate - Walk page tables and return the physical address
 *
 * Returns 0 if the page is not present at either level.
 */
uint32_t paging_translate(uint32_t virt) {
    uint32_t pd_idx = PAGE_DIR_INDEX(virt);
    uint32_t pt_idx = PAGE_TABLE_INDEX(virt);

    // Check PDE present
    if (!(kernel_directory->entries[pd_idx] & PAGE_PRESENT)) {
        return 0;
    }

    page_table_t* table = pd_tables[pd_idx];
    if (!table) {
        return 0;
    }

    // Check PTE present
    uint32_t pte = table->entries[pt_idx];
    if (!(pte & PAGE_PRESENT)) {
        return 0;
    }

    // Combine frame base + offset within page
    return PAGE_FRAME(pte) | (virt & 0xFFF);
}

// ─── Status / Debug ────────────────────────────────────────────────────────

/**
 * paging_status - Print page directory summary for the 'pagedir' shell command
 */
void paging_status(void) {
    if (!kernel_directory) {
        kprint("Paging: not initialized\n");
        return;
    }

    uint32_t present_tables = 0;
    uint32_t total_mapped_pages = 0;

    for (uint32_t i = 0; i < TABLES_PER_DIR; i++) {
        if (kernel_directory->entries[i] & PAGE_PRESENT) {
            present_tables++;

            page_table_t* table = pd_tables[i];
            if (table) {
                for (uint32_t j = 0; j < PAGES_PER_TABLE; j++) {
                    if (table->entries[j] & PAGE_PRESENT) {
                        total_mapped_pages++;
                    }
                }
            }
        }
    }

    uint32_t mapped_mb = (total_mapped_pages * 4) / 1024;  // pages * 4KB / 1024 = MB

    char buf[16];

    kprint("Page Directory Status:\n");

    kprint("  Present tables: ");
    uitoa(present_tables, buf, 10);
    kprint(buf);
    kprint(" / 1024\n");

    kprint("  Mapped pages:   ");
    uitoa(total_mapped_pages, buf, 10);
    kprint(buf);
    kprint("\n");

    kprint("  Mapped memory:  ");
    uitoa(mapped_mb, buf, 10);
    kprint(buf);
    kprint(" MB");

    if (total_mapped_pages == 4096 && present_tables == 4) {
        kprint(" (identity mapped 0x00000000 - 0x00FFFFFF)\n");
    } else {
        kprint("\n");
    }
}

// ─── Per-Process Address Space (Phase 15) ──────────────────────────────────

/**
 * paging_clone_directory - Create a per-process page directory
 *
 * 1. Allocates a new 4 KB page for the page directory
 * 2. For PDE 0: allocates a private page table clone (copy of kernel PT 0)
 *    so user code pages can be selectively marked PAGE_USER per-process
 *    without affecting the shared kernel mappings
 * 3. For PDE 1-(KERNEL_PDE_COUNT-1): copies the PDE verbatim (shared
 *    physical page tables, supervisor-only)
 * 4. Clears PDE entries above the kernel range (user space)
 */
page_directory_t* paging_clone_directory(uint32_t *out_phys) {
    /* Allocate a fresh page for the page directory */
    uint32_t dir_phys = alloc_page();
    if (dir_phys == 0) return 0;

    page_directory_t *dir = (page_directory_t *)dir_phys;
    memset(dir->entries, 0, sizeof(dir->entries));

    /* ── PDE 0: Private clone of kernel page table 0 ──
     * We need a private copy because user-code pages within 0-4 MB
     * must be marked PAGE_USER in this process only. */
    if (pd_tables[0]) {
        uint32_t pt0_phys = alloc_page();
        if (pt0_phys == 0) {
            free_page(dir_phys);
            return 0;
        }
        page_table_t *pt0_clone = (page_table_t *)pt0_phys;
        /* Copy all 1024 PTEs from the kernel's page table 0 */
        memcpy(pt0_clone->entries, pd_tables[0]->entries,
               sizeof(pt0_clone->entries));

        /* Install PDE 0 — supervisor-only at PDE level for now;
         * paging_map_page_in() will set PAGE_USER when mapping user pages */
        dir->entries[0] = pt0_phys | PAGE_PRESENT | PAGE_WRITABLE;
    }

    /* ── PDE 1 .. (KERNEL_PDE_COUNT-1): Shared kernel page tables ── */
    for (uint32_t i = 1; i < KERNEL_PDE_COUNT; i++) {
        dir->entries[i] = kernel_directory->entries[i];
        /* These remain supervisor-only — Ring 3 cannot touch kernel data */
    }

    /* ── PDE KERNEL_PDE_COUNT .. 1023: clear (populated per-process) ── */
    /* Already zeroed by memset above */

    if (out_phys) *out_phys = dir_phys;
    return dir;
}

/**
 * paging_map_page_in - Map a page in a specific page directory
 *
 * Works like paging_map_page() but targets @dir instead of the
 * global kernel_directory.  Under identity mapping physical == virtual,
 * so we derive the page_table_t* directly from the PDE.
 */
void paging_map_page_in(page_directory_t *dir, uint32_t virt,
                        uint32_t phys, uint32_t flags)
{
    uint32_t pd_idx = PAGE_DIR_INDEX(virt);
    uint32_t pt_idx = PAGE_TABLE_INDEX(virt);

    /* If no page table exists for this PDE slot, allocate one */
    if (!(dir->entries[pd_idx] & PAGE_PRESENT)) {
        uint32_t pt_phys = alloc_page();
        if (pt_phys == 0) {
            kprint("paging_map_page_in: out of memory for page table\n");
            return;
        }
        page_table_t *pt = (page_table_t *)pt_phys;
        memset(pt->entries, 0, sizeof(pt->entries));

        dir->entries[pd_idx] = pt_phys | PAGE_PRESENT | PAGE_WRITABLE
                              | (flags & PAGE_USER);
    } else if (flags & PAGE_USER) {
        /* PDE already exists — ensure PAGE_USER is propagated.
         * x86 checks both PDE and PTE User/Supervisor bits. */
        dir->entries[pd_idx] |= PAGE_USER;
    }

    /* Derive the page_table_t* from the PDE (identity mapping) */
    page_table_t *table = (page_table_t *)(dir->entries[pd_idx] & PAGE_FRAME_MASK);

    /* Set the PTE */
    table->entries[pt_idx] = (phys & PAGE_FRAME_MASK)
                            | (flags & 0xFFF)
                            | PAGE_PRESENT;
}

/**
 * paging_free_directory - Free a per-process page directory
 *
 * Walks the directory:
 *   - PDE 0: free the private page table clone (but NOT the physical
 *     frames, which belong to the kernel identity map)
 *   - PDE 1 .. (KERNEL_PDE_COUNT-1): skip (shared kernel page tables)
 *   - PDE KERNEL_PDE_COUNT .. 1023: for each present entry, walk the
 *     page table and free every mapped physical page, then free the
 *     page table page itself
 *   - Finally, free the page directory page
 */
void paging_free_directory(page_directory_t *dir) {
    if (!dir) return;

    /* ── PDE 0: private clone — free only the page table page ── */
    if (dir->entries[0] & PAGE_PRESENT) {
        uint32_t pt0_phys = dir->entries[0] & PAGE_FRAME_MASK;
        /* Do NOT free individual frames — they belong to the kernel */
        free_page(pt0_phys);
    }

    /* ── PDE 1 .. (KERNEL_PDE_COUNT-1): shared — skip ── */

    /* ── PDE KERNEL_PDE_COUNT .. 1023: per-process user pages ── */
    for (uint32_t i = KERNEL_PDE_COUNT; i < TABLES_PER_DIR; i++) {
        if (!(dir->entries[i] & PAGE_PRESENT)) continue;

        uint32_t pt_phys = dir->entries[i] & PAGE_FRAME_MASK;
        page_table_t *pt = (page_table_t *)pt_phys;

        /* Free every mapped physical page in this table */
        for (uint32_t j = 0; j < PAGES_PER_TABLE; j++) {
            if (pt->entries[j] & PAGE_PRESENT) {
                uint32_t frame = pt->entries[j] & PAGE_FRAME_MASK;
                free_page(frame);
            }
        }

        /* Free the page table page itself */
        free_page(pt_phys);
    }

    /* ── Free the directory page ── */
    free_page((uint32_t)dir);
}

/**
 * paging_get_kernel_cr3 - Return the kernel page directory physical address
 */
uint32_t paging_get_kernel_cr3(void) {
    return pd_physical_address;
}
