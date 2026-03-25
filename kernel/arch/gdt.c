#include "gdt.h"

// GDT with 3 entries: null, code, data
#define GDT_ENTRIES 3

struct gdt_entry gdt[GDT_ENTRIES];
struct gdt_ptr gp;

// External assembly function to load GDT
extern void gdt_flush(uint32_t);

// Set a GDT gate
static void gdt_set_gate(int32_t num, uint32_t base, uint32_t limit, uint8_t access, uint8_t gran) {
    gdt[num].base_low = (base & 0xFFFF);
    gdt[num].base_middle = (base >> 16) & 0xFF;
    gdt[num].base_high = (base >> 24) & 0xFF;
    
    gdt[num].limit_low = (limit & 0xFFFF);
    gdt[num].granularity = (limit >> 16) & 0x0F;
   gdt[num].granularity |= gran & 0xF0;
    gdt[num].access = access;
}

// Initialize the GDT
void gdt_init(void) {
    gp.limit = (sizeof(struct gdt_entry) * GDT_ENTRIES) - 1;
    gp.base = (uint32_t)&gdt;
    
    // Null descriptor
    gdt_set_gate(0, 0, 0, 0, 0);
    
    // Code segment: base=0, limit=0xFFFFFFFF, access=0x9A, granularity=0xCF
    // Access: Present=1, DPL=00, Type=1010 (code, execute/read)
    // Granularity: G=1 (4KB), D=1 (32-bit), L=0, bits 16-19 of limit=1111
    gdt_set_gate(1, 0, 0xFFFFFFFF, 0x9A, 0xCF);
    
    // Data segment: base=0, limit=0xFFFFFFFF, access=0x92, granularity=0xCF
    // Access: Present=1, DPL=00, Type=0010 (data, read/write)
    gdt_set_gate(2, 0, 0xFFFFFFFF, 0x92, 0xCF);
    
    // Load the GDT
    gdt_flush((uint32_t)&gp);
}
