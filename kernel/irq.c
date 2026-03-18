#include "irq.h"
#include "isr.h"
#include "pic.h"

/*
 * IRQ (Interrupt Request) Handler
 * 
 * Handles hardware interrupts (IRQs 0-15 mapped to INT 32-47).
 * This function:
 *   1. Checks if a custom handler is registered for this IRQ
 *   2. Calls the handler if it exists
 *   3. Sends End-Of-Interrupt (EOI) to the PIC
 * 
 * The EOI tells the PIC that we've finished handling the interrupt,
 * allowing it to send more interrupts.
 */
void irq_handler(registers_t* regs) {
    // Calculate the IRQ number (0-15) from the interrupt number (32-47)
    uint8_t irq = regs->int_no - 32;

    // Call custom handler if one is registered
    if (interrupt_handlers[regs->int_no] != 0) {
        interrupt_handlers[regs->int_no](regs);
    }

    // Send End-Of-Interrupt signal to PIC
    // This MUST be done after handling the interrupt
    pic_send_eoi(irq);
}
