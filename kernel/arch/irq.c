#include "irq.h"
#include "isr.h"
#include "pic.h"

/*
 * IRQ (Interrupt Request) Handler
 * 
 * Handles hardware interrupts (IRQs 0-15 mapped to INT 32-47).
 * This function:
 *   1. Sends End-Of-Interrupt (EOI) to the PIC early
 *   2. Calls the registered handler if one exists
 * 
 * EOI is sent BEFORE the handler because the timer handler may perform
 * a context switch (schedule → task_switch).  If the switch happens
 * before EOI, the PIC never un-masks the timer line, freezing all
 * further timer interrupts.  Sending EOI first is safe because
 * interrupts are disabled (IF=0) throughout the ISR stub, so no
 * nested interrupt can arrive until iret restores EFLAGS.
 */
void irq_handler(registers_t* regs) {
    // Calculate the IRQ number (0-15) from the interrupt number (32-47)
    uint8_t irq = regs->int_no - 32;

    // Send End-Of-Interrupt signal to PIC BEFORE the handler
    // so a context switch inside the handler doesn't starve the IRQ line
    pic_send_eoi(irq);

    // Call custom handler if one is registered
    if (interrupt_handlers[regs->int_no] != 0) {
        interrupt_handlers[regs->int_no](regs);
    }
}
