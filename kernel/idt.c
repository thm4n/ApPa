#include "idt.h"
#include "isr.h"

#define IDT_ENTRIES 256
#define KERNEL_CODE_SEG 0x08
#define IDT_FLAG_PRESENT    0x80
#define IDT_FLAG_RING0      0x00
#define IDT_FLAG_RING3      0x60
#define IDT_FLAG_GATE_INT   0x0E  // 32-bit interrupt gate
#define IDT_FLAG_GATE_TRAP  0x0F  // 32-bit trap gate

// IDT array - 256 entries
struct InterruptDescriptor32 idt[IDT_ENTRIES];

// IDT pointer structure for lidt instruction
struct idt_ptr {
    uint16_t limit;
    uint32_t base;
} __attribute__((packed));

struct idt_ptr idtp;

// Set a gate in the IDT
// num: interrupt number (0-255)
// handler: address of the interrupt handler function
// selector: code segment selector (0x08 for kernel code)
// flags: type and attributes (0x8E = present, ring 0, 32-bit interrupt gate)
void idt_set_gate(uint8_t num, uint32_t handler, uint16_t selector, uint8_t flags) {
    idt[num].offset_1 = handler & 0xFFFF;         // Low 16 bits of handler address
    idt[num].offset_2 = (handler >> 16) & 0xFFFF; // High 16 bits of handler address
    idt[num].selector = selector;                  // Code segment selector
    idt[num].zero = 0;                             // Always 0
    idt[num].type_attributes = flags;              // Type and attributes
}

// Load the IDT using the lidt instruction
extern void idt_load(uint32_t idt_ptr);

// Initialize the IDT
void idt_init(void) {
    // Set up the IDT pointer
    idtp.limit = (sizeof(struct InterruptDescriptor32) * IDT_ENTRIES) - 1;
    idtp.base = (uint32_t)&idt;

    // Clear the IDT - set all entries to 0
    for (int i = 0; i < IDT_ENTRIES; i++) {
        idt_set_gate(i, 0, 0, 0);
    }

    // Set up CPU exception handlers (ISR 0-31)
    uint8_t flags = IDT_FLAG_PRESENT | IDT_FLAG_RING0 | IDT_FLAG_GATE_INT;
    idt_set_gate(0,  (uint32_t)isr0,  KERNEL_CODE_SEG, flags);
    idt_set_gate(1,  (uint32_t)isr1,  KERNEL_CODE_SEG, flags);
    idt_set_gate(2,  (uint32_t)isr2,  KERNEL_CODE_SEG, flags);
    idt_set_gate(3,  (uint32_t)isr3,  KERNEL_CODE_SEG, flags);
    idt_set_gate(4,  (uint32_t)isr4,  KERNEL_CODE_SEG, flags);
    idt_set_gate(5,  (uint32_t)isr5,  KERNEL_CODE_SEG, flags);
    idt_set_gate(6,  (uint32_t)isr6,  KERNEL_CODE_SEG, flags);
    idt_set_gate(7,  (uint32_t)isr7,  KERNEL_CODE_SEG, flags);
    idt_set_gate(8,  (uint32_t)isr8,  KERNEL_CODE_SEG, flags);
    idt_set_gate(9,  (uint32_t)isr9,  KERNEL_CODE_SEG, flags);
    idt_set_gate(10, (uint32_t)isr10, KERNEL_CODE_SEG, flags);
    idt_set_gate(11, (uint32_t)isr11, KERNEL_CODE_SEG, flags);
    idt_set_gate(12, (uint32_t)isr12, KERNEL_CODE_SEG, flags);
    idt_set_gate(13, (uint32_t)isr13, KERNEL_CODE_SEG, flags);
    idt_set_gate(14, (uint32_t)isr14, KERNEL_CODE_SEG, flags);
    idt_set_gate(15, (uint32_t)isr15, KERNEL_CODE_SEG, flags);
    idt_set_gate(16, (uint32_t)isr16, KERNEL_CODE_SEG, flags);
    idt_set_gate(17, (uint32_t)isr17, KERNEL_CODE_SEG, flags);
    idt_set_gate(18, (uint32_t)isr18, KERNEL_CODE_SEG, flags);
    idt_set_gate(19, (uint32_t)isr19, KERNEL_CODE_SEG, flags);
    idt_set_gate(20, (uint32_t)isr20, KERNEL_CODE_SEG, flags);
    idt_set_gate(21, (uint32_t)isr21, KERNEL_CODE_SEG, flags);
    idt_set_gate(22, (uint32_t)isr22, KERNEL_CODE_SEG, flags);
    idt_set_gate(23, (uint32_t)isr23, KERNEL_CODE_SEG, flags);
    idt_set_gate(24, (uint32_t)isr24, KERNEL_CODE_SEG, flags);
    idt_set_gate(25, (uint32_t)isr25, KERNEL_CODE_SEG, flags);
    idt_set_gate(26, (uint32_t)isr26, KERNEL_CODE_SEG, flags);
    idt_set_gate(27, (uint32_t)isr27, KERNEL_CODE_SEG, flags);
    idt_set_gate(28, (uint32_t)isr28, KERNEL_CODE_SEG, flags);
    idt_set_gate(29, (uint32_t)isr29, KERNEL_CODE_SEG, flags);
    idt_set_gate(30, (uint32_t)isr30, KERNEL_CODE_SEG, flags);
    idt_set_gate(31, (uint32_t)isr31, KERNEL_CODE_SEG, flags);

    // TODO: Remap PIC and set up IRQ handlers (Phase 2, 3)

    // Load the IDT
    idt_load((uint32_t)&idtp);
}
