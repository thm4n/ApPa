#ifndef IRQ_H
#define IRQ_H

#include "../../klibc/stdint.h"
#include "isr.h"

/*
 * IRQ (Interrupt Request) Handler Interface
 * 
 * Hardware interrupts from devices (keyboard, timer, disk, etc.)
 * After PIC remapping: IRQs 0-15 map to interrupts 32-47
 */

// C handler called from assembly (irq_stubs.asm)
void irq_handler(registers_t* regs);

// External IRQ stub declarations (defined in irq_stubs.asm)
// IRQs 0-15 map to interrupts 32-47 after PIC remapping
extern void irq0(void);   // IRQ 0  - Timer
extern void irq1(void);   // IRQ 1  - Keyboard
extern void irq2(void);   // IRQ 2  - Cascade
extern void irq3(void);   // IRQ 3  - COM2
extern void irq4(void);   // IRQ 4  - COM1
extern void irq5(void);   // IRQ 5  - LPT2/Sound
extern void irq6(void);   // IRQ 6  - Floppy
extern void irq7(void);   // IRQ 7  - LPT1
extern void irq8(void);   // IRQ 8  - RTC
extern void irq9(void);   // IRQ 9  - Free
extern void irq10(void);  // IRQ 10 - Free
extern void irq11(void);  // IRQ 11 - Free
extern void irq12(void);  // IRQ 12 - PS/2 Mouse
extern void irq13(void);  // IRQ 13 - Coprocessor
extern void irq14(void);  // IRQ 14 - Primary ATA
extern void irq15(void);  // IRQ 15 - Secondary ATA

#endif
