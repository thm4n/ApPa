/**
 * tss.c - Task State Segment implementation
 *
 * Sets up the x86 TSS so the CPU knows where the kernel stack is.
 * The TSS descriptor occupies GDT entry 5 (selector 0x28).
 */

#include "tss.h"
#include "gdt.h"
#include "../../klibc/string.h"

// The single kernel TSS instance
static tss_entry_t tss;

// External assembly routine: loads the TSS selector into TR
extern void tss_flush(void);

/**
 * tss_init - Install and load the Task State Segment
 * @kernel_ss:  Kernel data segment selector (0x10)
 * @kernel_esp: Initial kernel stack top
 */
void tss_init(uint32_t kernel_ss, uint32_t kernel_esp) {
    // Zero out the TSS first
    memset(&tss, 0, sizeof(tss_entry_t));

    // Set the kernel stack segment and pointer
    tss.ss0  = kernel_ss;
    tss.esp0 = kernel_esp;

    // Set the I/O map base to the size of the TSS (no I/O bitmap)
    tss.iomap_base = sizeof(tss_entry_t);

    // Install the TSS descriptor into GDT entry 5
    // The GDT helper is exposed through gdt.h
    uint32_t base  = (uint32_t)&tss;
    uint32_t limit = sizeof(tss_entry_t) - 1;

    // Access byte 0xE9: Present(1) | DPL=3(11) | Type=TSS 32-bit available(01001)
    // DPL=3 allows user-mode tasks to trigger the TSS on interrupt
    // Granularity 0x00: byte granularity, 16-bit segment
    gdt_set_gate_ext(5, base, limit, 0xE9, 0x00);

    // Reload the GDT (size changed from 3 to 6 entries)
    gdt_reload();

    // Load the TSS selector into the Task Register
    tss_flush();
}

/**
 * tss_set_kernel_stack - Update kernel stack pointer in the TSS
 * @esp0: New top-of-stack for this task's kernel stack
 *
 * Must be called on every context switch before entering the new task.
 */
void tss_set_kernel_stack(uint32_t esp0) {
    tss.esp0 = esp0;
}
