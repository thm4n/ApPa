#ifndef KEYBOARD_H
#define KEYBOARD_H

#include "../libc/stdint.h"

/*
 * PS/2 Keyboard Driver
 * 
 * Handles keyboard interrupts (IRQ1 / INT 33) and translates
 * hardware scan codes to ASCII characters.
 */

// Keyboard I/O ports
#define KEYBOARD_DATA_PORT    0x60  // Read scan codes
#define KEYBOARD_STATUS_PORT  0x64  // Status/command port

/*
 * Scancode to ASCII Translation Table
 * 
 * Maps keyboard scan codes (make codes) to ASCII characters.
 * Scan codes are hardware-specific key identifiers.
 * 
 * US QWERTY layout, lowercase, no modifier key support.
 * 
 * Index = scan code (0x00-0x7F)
 * Value = ASCII character (0 = unprintable/special key)
 * 
 * Scan code bit 7:
 *   - Clear (0x00-0x7F): Key pressed (make code)
 *   - Set   (0x80-0xFF): Key released (break code)
 */
static const char scancode_to_ascii[128] = {
    0,    0,    '1',  '2',  '3',  '4',  '5',  '6',   // 0x00-0x07
    '7',  '8',  '9',  '0',  '-',  '=',  '\b', '\t',  // 0x08-0x0F (backspace, tab)
    'q',  'w',  'e',  'r',  't',  'y',  'u',  'i',   // 0x10-0x17
    'o',  'p',  '[',  ']',  '\n', 0,    'a',  's',   // 0x18-0x1F (enter, ctrl)
    'd',  'f',  'g',  'h',  'j',  'k',  'l',  ';',   // 0x20-0x27
    '\'', '`',  0,    '\\', 'z',  'x',  'c',  'v',   // 0x28-0x2F (left shift)
    'b',  'n',  'm',  ',',  '.',  '/',  0,    '*',   // 0x30-0x37 (right shift, keypad *)
    0,    ' ',  0,    0,    0,    0,    0,    0,     // 0x38-0x3F (alt, caps, F1-F5)
    0,    0,    0,    0,    0,    0,    0,    '7',   // 0x40-0x47 (F6-F10, num lock, scroll lock, keypad 7)
    '8',  '9',  '-',  '4',  '5',  '6',  '+',  '1',   // 0x48-0x4F (keypad)
    '2',  '3',  '0',  '.',  0,    0,    0,    0,     // 0x50-0x57 (keypad, F11, F12)
    0,    0,    0,    0,    0,    0,    0,    0,     // 0x58-0x5F
    0,    0,    0,    0,    0,    0,    0,    0,     // 0x60-0x67
    0,    0,    0,    0,    0,    0,    0,    0,     // 0x68-0x6F
    0,    0,    0,    0,    0,    0,    0,    0,     // 0x70-0x77
    0,    0,    0,    0,    0,    0,    0,    0      // 0x78-0x7F
};

// Function declarations
void keyboard_init(void);

#endif
