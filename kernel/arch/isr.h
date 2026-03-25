#ifndef ISR_H
#define ISR_H

#include "../../libc/stdint.h"

// Structure matching the stack after isr_common_stub saves state
typedef struct {
    uint32_t ds;                                      // Data segment pushed by us
    uint32_t edi, esi, ebp, esp, ebx, edx, ecx, eax;  // Pushed by pusha
    uint32_t int_no, err_code;                        // Interrupt number and error code
    uint32_t eip, cs, eflags, useresp, ss;            // Pushed by CPU automatically
} registers_t;

// ISR handler function type
typedef void (*isr_handler_t)(registers_t*);

// Array of interrupt handlers (shared by ISR and IRQ)
extern isr_handler_t interrupt_handlers[256];

// C handler called from assembly
void isr_handler(registers_t* regs);

// Register a handler for an interrupt
void register_interrupt_handler(uint8_t n, isr_handler_t handler);

// External ISR stub declarations (defined in isr.asm)
extern void isr0(void);
extern void isr1(void);
extern void isr2(void);
extern void isr3(void);
extern void isr4(void);
extern void isr5(void);
extern void isr6(void);
extern void isr7(void);
extern void isr8(void);
extern void isr9(void);
extern void isr10(void);
extern void isr11(void);
extern void isr12(void);
extern void isr13(void);
extern void isr14(void);
extern void isr15(void);
extern void isr16(void);
extern void isr17(void);
extern void isr18(void);
extern void isr19(void);
extern void isr20(void);
extern void isr21(void);
extern void isr22(void);
extern void isr23(void);
extern void isr24(void);
extern void isr25(void);
extern void isr26(void);
extern void isr27(void);
extern void isr28(void);
extern void isr29(void);
extern void isr30(void);
extern void isr31(void);

#endif
