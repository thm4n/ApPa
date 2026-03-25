#include "gdt.h"

// GDT with 6 entries: null, kernel code, kernel data, user code, user data, TSS
struct gdt_entry gdt[GDT_ENTRIES];
struct gdt_ptr gp;

// External assembly function to load GDT
extern void gdt_flush(uint32_t);

// Set a GDT gate (internal helper)
static void gdt_set_gate(int32_t num, uint32_t base, uint32_t limit, uint8_t access, uint8_t gran) {
    gdt[num].base_low = (base & 0xFFFF);
    gdt[num].base_middle = (base >> 16) & 0xFF;
    gdt[num].base_high = (base >> 24) & 0xFF;
    
    gdt[num].limit_low = (limit & 0xFFFF);
    gdt[num].granularity = (limit >> 16) & 0x0F;
   gdt[num].granularity |= gran & 0xF0;
    gdt[num].access = access;
}

// Public wrapper so tss.c can install the TSS descriptor
void gdt_set_gate_ext(int32_t num, uint32_t base, uint32_t limit,
                       uint8_t access, uint8_t gran) {
    gdt_set_gate(num, base, limit, access, gran);
}

// Reload the GDT pointer (e.g. after adding the TSS entry)
void gdt_reload(void) {
    gp.limit = (sizeof(struct gdt_entry) * GDT_ENTRIES) - 1;
    gp.base = (uint32_t)&gdt;
    gdt_flush((uint32_t)&gp);
}

// Initialize the GDT
void gdt_init(void) {
    gp.limit = (sizeof(struct gdt_entry) * GDT_ENTRIES) - 1;
    gp.base = (uint32_t)&gdt;
    
    // Entry 0: Null descriptor
    gdt_set_gate(0, 0, 0, 0, 0);
    
    // Entry 1: Kernel code segment (Ring 0)
    // Access 0x9A: Present=1, DPL=00, Type=1010 (code, execute/read)
    // Granularity 0xCF: G=1 (4KB), D=1 (32-bit), limit bits 16-19=F
    gdt_set_gate(1, 0, 0xFFFFFFFF, 0x9A, 0xCF);
    
    // Entry 2: Kernel data segment (Ring 0)
    // Access 0x92: Present=1, DPL=00, Type=0010 (data, read/write)
    gdt_set_gate(2, 0, 0xFFFFFFFF, 0x92, 0xCF);

    // Entry 3: User code segment (Ring 3)
    // Access 0xFA: Present=1, DPL=11, Type=1010 (code, execute/read)
    gdt_set_gate(3, 0, 0xFFFFFFFF, 0xFA, 0xCF);

    // Entry 4: User data segment (Ring 3)
    // Access 0xF2: Present=1, DPL=11, Type=0010 (data, read/write)
    gdt_set_gate(4, 0, 0xFFFFFFFF, 0xF2, 0xCF);

    // Entry 5: TSS - left zeroed here, filled by tss_init() later
    gdt_set_gate(5, 0, 0, 0, 0);

    // Load the GDT
    gdt_flush((uint32_t)&gp);
}
