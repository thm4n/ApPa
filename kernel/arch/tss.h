/**
 * tss.h - Task State Segment
 *
 * The x86 TSS is a hardware-defined 104-byte structure. In our kernel
 * we only use it to store the kernel stack pointer (ss0:esp0) so that
 * when an interrupt fires from Ring 3 (future) or during a context
 * switch, the CPU knows where to find the kernel stack.
 *
 * The TSS lives in GDT entry 5 (selector 0x28).
 */

#ifndef TSS_H
#define TSS_H

#include "../../libc/stdint.h"

/**
 * tss_entry_t - x86 Task State Segment (hardware layout)
 *
 * We only actively use esp0 and ss0. The remaining fields are
 * zeroed but must exist because the CPU expects the full 104 bytes.
 */
typedef struct {
    uint32_t prev_tss;   // Link to previous TSS (unused)
    uint32_t esp0;       // Stack pointer for Ring 0
    uint32_t ss0;        // Stack segment for Ring 0
    uint32_t esp1;       // Ring 1 stack pointer (unused)
    uint32_t ss1;        // Ring 1 stack segment (unused)
    uint32_t esp2;       // Ring 2 stack pointer (unused)
    uint32_t ss2;        // Ring 2 stack segment (unused)
    uint32_t cr3;        // Page directory base (unused, we switch CR3 manually)
    uint32_t eip;
    uint32_t eflags;
    uint32_t eax;
    uint32_t ecx;
    uint32_t edx;
    uint32_t ebx;
    uint32_t esp;
    uint32_t ebp;
    uint32_t esi;
    uint32_t edi;
    uint32_t es;         // Segment selectors
    uint32_t cs;
    uint32_t ss;
    uint32_t ds;
    uint32_t fs;
    uint32_t gs;
    uint32_t ldt;        // LDT selector (unused)
    uint16_t trap;       // Debug trap flag
    uint16_t iomap_base; // I/O map base address
} __attribute__((packed)) tss_entry_t;

/**
 * tss_init - Install the TSS into GDT entry 5 and load it
 * @kernel_ss:  Kernel stack segment selector (normally 0x10)
 * @kernel_esp: Initial kernel stack pointer (top of kernel stack)
 *
 * Must be called after gdt_init().
 */
void tss_init(uint32_t kernel_ss, uint32_t kernel_esp);

/**
 * tss_set_kernel_stack - Update the kernel stack in the TSS
 * @esp0: New kernel stack pointer
 *
 * Called on every context switch so the CPU uses the correct
 * kernel stack when handling interrupts for the current task.
 */
void tss_set_kernel_stack(uint32_t esp0);

#endif // TSS_H
