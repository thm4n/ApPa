#include "pic.h"
#include "../drivers/ports.h"

/*
 * 8259 PIC (Programmable Interrupt Controller) Driver
 * 
 * Manages hardware interrupt remapping and acknowledgment.
 * The PIC translates hardware interrupt requests (IRQs) into 
 * CPU interrupt numbers.
 */

/**
 * io_wait - Small delay for old hardware
 * 
 * The PIC is slow hardware and needs time between commands.
 * Writing to port 0x80 (POST diagnostic port) provides a safe delay.
 */
static void io_wait(void) {
    port_byte_out(0x80, 0);
}

/**
 * pic_remap - Remap PIC interrupt vectors
 * @offset1: Vector offset for master PIC (IRQ 0-7)
 * @offset2: Vector offset for slave PIC (IRQ 8-15)
 * 
 * By default, IRQs 0-7 map to interrupts 8-15, which conflicts with
 * CPU exception handlers. This function remaps them to avoid conflicts.
 * 
 * Typical usage: pic_remap(32, 40)
 *   - Master PIC: IRQ 0-7  → INT 32-39
 *   - Slave PIC:  IRQ 8-15 → INT 40-47
 * 
 * The initialization sequence uses ICW (Initialization Command Words):
 *   ICW1: Start initialization, indicate ICW4 is coming
 *   ICW2: Set base interrupt vector number
 *   ICW3: Configure master/slave cascade setup
 *   ICW4: Set 8086 mode and other options
 */
void pic_remap(uint8_t offset1, uint8_t offset2) {
    uint8_t mask1, mask2;
    
    // Save current interrupt masks
    mask1 = port_byte_in(PIC1_DATA);
    mask2 = port_byte_in(PIC2_DATA);
    
    // ICW1: Start initialization sequence (expect ICW4)
    port_byte_out(PIC1_COMMAND, ICW1_INIT | ICW1_ICW4);
    io_wait();
    port_byte_out(PIC2_COMMAND, ICW1_INIT | ICW1_ICW4);
    io_wait();
    
    // ICW2: Set vector offset (base interrupt number)
    port_byte_out(PIC1_DATA, offset1);  // Master: IRQ 0-7 start at offset1
    io_wait();
    port_byte_out(PIC2_DATA, offset2);  // Slave: IRQ 8-15 start at offset2
    io_wait();
    
    // ICW3: Configure cascade mode
    port_byte_out(PIC1_DATA, 0x04);     // Master: Slave PIC on IRQ2 (binary 0000 0100)
    io_wait();
    port_byte_out(PIC2_DATA, 0x02);     // Slave: Cascade identity = 2
    io_wait();
    
    // ICW4: Set 8086/88 mode
    port_byte_out(PIC1_DATA, ICW4_8086);
    io_wait();
    port_byte_out(PIC2_DATA, ICW4_8086);
    io_wait();
    
    // Restore saved interrupt masks
    port_byte_out(PIC1_DATA, mask1);
    port_byte_out(PIC2_DATA, mask2);
}

/**
 * pic_send_eoi - Send End-Of-Interrupt signal
 * @irq: IRQ number (0-15)
 * 
 * Must be called at the end of every IRQ handler to notify the PIC
 * that interrupt processing is complete. Until EOI is sent, the PIC
 * will block lower-priority interrupts.
 * 
 * For IRQs 8-15 (slave PIC), we must send EOI to both PICs because
 * the slave is cascaded through the master's IRQ2.
 */
void pic_send_eoi(uint8_t irq) {
    // IRQs 8-15 are on slave PIC, which cascades through master
    if (irq >= 8) {
        port_byte_out(PIC2_COMMAND, PIC_EOI);
    }
    
    // Always send EOI to master (all IRQs go through it)
    port_byte_out(PIC1_COMMAND, PIC_EOI);
}
