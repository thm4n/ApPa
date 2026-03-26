#ifndef PIC_H
#define PIC_H

#include "../../klibc/stdint.h"

/*
 * 8259 Programmable Interrupt Controller (PIC) Driver
 * 
 * The PIC manages hardware interrupt requests (IRQs) from devices like
 * keyboard, timer, disk controllers, etc. Two PICs are cascaded:
 *   - Master PIC: IRQs 0-7
 *   - Slave PIC: IRQs 8-15 (connected to Master's IRQ 2)
 */

// Master PIC I/O port addresses
#define PIC1_COMMAND    0x20    // Master PIC command port
#define PIC1_DATA       0x21    // Master PIC data port

// Slave PIC I/O port addresses
#define PIC2_COMMAND    0xA0    // Slave PIC command port
#define PIC2_DATA       0xA1    // Slave PIC data port

// Initialization Command Word 1 (ICW1)
#define ICW1_INIT       0x10    // Initialization command
#define ICW1_ICW4       0x01    // ICW4 will be sent

// Initialization Command Word 4 (ICW4)
#define ICW4_8086       0x01    // 8086/88 mode (not 8080 mode)

// End of Interrupt command
#define PIC_EOI         0x20    // End-Of-Interrupt command code

// PIC remapping offsets
#define PIC1_OFFSET     0x20    // Master PIC: IRQ 0-7 → INT 32-39
#define PIC2_OFFSET     0x28    // Slave PIC: IRQ 8-15 → INT 40-47

// Function declarations
void pic_remap(uint8_t offset1, uint8_t offset2);
void pic_send_eoi(uint8_t irq);
void irq_clear_mask(uint8_t irq);
void irq_set_mask(uint8_t irq);

#endif // PIC_H
