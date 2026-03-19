#include "keyboard.h"
#include "screen.h"
#include "ports.h"
#include "../kernel/isr.h"
#include "../kernel/klog.h"

/*
 * PS/2 Keyboard Driver
 * 
 * Handles keyboard interrupts and displays typed characters on screen.
 */

/**
 * keyboard_handler - IRQ1 interrupt handler
 * @regs: CPU register state at time of interrupt
 * 
 * Called automatically when a key is pressed or released.
 * Reads the scan code from the keyboard controller (port 0x60),
 * translates it to ASCII, and displays the character.
 * 
 * Scan codes:
 *   - 0x00-0x7F: Key pressed (make code)
 *   - 0x80-0xFF: Key released (break code)
 */
static void keyboard_handler(registers_t* regs) {
    // Suppress unused parameter warning
    (void)regs;
    
    // Read scan code from keyboard controller
    // This also clears the keyboard's internal buffer
    uint8_t scancode = port_byte_in(KEYBOARD_DATA_PORT);
    
    // Check if this is a key release event (bit 7 set)
    if (scancode & 0x80) {
        // Ignore key release events for now
        // Advanced keyboards might need this for key repeat, chords, etc.
        return;
    }
    
    // Handle special function keys
    if (scancode == 0x3B) {  // F1 key
        kprint("\n");
        klog_dump();
        kprint("> ");
        return;
    }
    
    // Translate scan code to ASCII character
    char ascii = scancode_to_ascii[scancode];
    
    // Only print if it's a printable character
    if (ascii != 0) {
        // Handle special characters
        if (ascii == '\b') {
            // Backspace: move cursor back and erase character
            kprint_backspace();
        } else {
            // Normal character: print it
            char str[2] = {ascii, '\0'};
            kprint(str);
        }
    }
}

/**
 * keyboard_init - Initialize keyboard driver
 * 
 * Registers the keyboard interrupt handler for IRQ1.
 * After PIC remapping, IRQ1 maps to interrupt 33.
 * 
 * This must be called after:
 *   - idt_init() has set up the IDT
 *   - pic_remap() has remapped the PIC
 * 
 * And before:
 *   - Interrupts are enabled with sti
 */
void keyboard_init(void) {
    // IRQ1 maps to interrupt 33 after PIC remapping (32 + 1)
    register_interrupt_handler(33, keyboard_handler);
}
