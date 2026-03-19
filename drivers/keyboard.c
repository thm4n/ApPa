#include "keyboard.h"
#include "screen.h"
#include "ports.h"
#include "../kernel/isr.h"
#include "../kernel/klog.h"
#include "../kernel/pic.h"
#include "../kernel/shell.h"

/*
 * PS/2 Keyboard Driver
 * 
 * Handles keyboard interrupts and displays typed characters on screen.
 */

// Track modifier key states
static uint8_t shift_pressed = 0;

// Timer tick counter for IRQ0
static uint32_t timer_ticks = 0;

/**
 * timer_handler - IRQ0 interrupt handler
 * @regs: CPU register state at time of interrupt
 * 
 * Called automatically by the PIT (Programmable Interval Timer).
 * Updates a counter on screen to verify interrupts are working.
 */
static void timer_handler(registers_t *regs) {
	// Suppress unused parameter warning
	(void)regs;
	
	timer_ticks++;
	
	// Update counter on screen every tick
	unsigned char *video = (unsigned char*)0xb8000;
	video[158] = '0' + (timer_ticks % 10);
	video[159] = 0x0C;  // Bright red on black
}

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
        // Handle shift key release
        uint8_t release_code = scancode & 0x7F;
        if (release_code == KEY_LSHIFT || release_code == KEY_RSHIFT) {
            shift_pressed = 0;
        }
        return;
    }
    
    // Handle shift key press
    if (scancode == KEY_LSHIFT || scancode == KEY_RSHIFT) {
        shift_pressed = 1;
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
    
    // Only process if it's a printable character
    if (ascii != 0) {
        // Apply shift if shift key is held
        if (shift_pressed) {
            ascii = apply_shift(ascii);
        }
        
        // Pass character to shell for processing
        shell_input(ascii);
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
	// Register timer handler for IRQ0 (interrupt 32 after PIC remap)
	register_interrupt_handler(32, timer_handler);
	
	// Register keyboard handler for IRQ1 (interrupt 33 after PIC remap)
	register_interrupt_handler(33, keyboard_handler);
    
    // Properly configure PS/2 keyboard controller to enable interrupts
    
    // Step 1: Disable devices while we configure
    port_byte_out(KEYBOARD_STATUS_PORT, 0xAD);  // Disable keyboard
    port_byte_out(KEYBOARD_STATUS_PORT, 0xA7);  // Disable mouse
    
    // Step 2: Flush output buffer
    port_byte_in(KEYBOARD_DATA_PORT);
    
    // Step 3: Read current configuration
    port_byte_out(KEYBOARD_STATUS_PORT, 0x20);  // Read command byte
    while (!(port_byte_in(KEYBOARD_STATUS_PORT) & 0x01));  // Wait for data
    uint8_t config = port_byte_in(KEYBOARD_DATA_PORT);
    
    // Step 4: Enable keyboard interrupt (bit 0) and translation (bit 6)
    config |= 0x01;   // Enable keyboard interrupt
    config &= ~0x10;  // Enable keyboard clock
    
    // Step 5: Write configuration back
    port_byte_out(KEYBOARD_STATUS_PORT, 0x60);  // Write command byte
    port_byte_out(KEYBOARD_DATA_PORT, config);
    
    // Step 6: Re-enable keyboard
    port_byte_out(KEYBOARD_STATUS_PORT, 0xAE);  // Enable keyboard
}
