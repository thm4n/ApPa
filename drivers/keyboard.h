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

// Special key scan codes
#define KEY_LSHIFT   0x2A   // Left shift
#define KEY_RSHIFT   0x36   // Right shift
#define KEY_LCTRL    0x1D   // Left control
#define KEY_RCTRL    0x9D   // Right control (break code)
#define KEY_LALT     0x38   // Left alt
#define KEY_RALT     0xB8   // Right alt (break code)
#define KEY_CAPSLOCK 0x3A   // Caps lock

/*
 * Scancode to ASCII Translation Table
 * 
 * Maps keyboard scan codes (make codes) to ASCII characters.
 * Scan codes are hardware-specific key identifiers.
 * 
 * US QWERTY layout, lowercase (unshifted).
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

/*
 * apply_shift - Convert character to shifted version
 * @c: Character to shift
 * 
 * For letters: converts to uppercase
 * For symbols: returns shifted symbol
 * For others: returns unchanged
 */
static inline char apply_shift(char c) {
    // Letters: convert to uppercase (a-z -> A-Z)
    if (c >= 'a' && c <= 'z') {
        return c - 32;
    }
    
    // Symbols that change when shifted
    switch (c) {
        case '1': return '!';
        case '2': return '@';
        case '3': return '#';
        case '4': return '$';
        case '5': return '%';
        case '6': return '^';
        case '7': return '&';
        case '8': return '*';
        case '9': return '(';
        case '0': return ')';
        case '-': return '_';
        case '=': return '+';
        case '[': return '{';
        case ']': return '}';
        case ';': return ':';
        case '\'': return '"';
        case '`': return '~';
        case '\\': return '|';
        case ',': return '<';
        case '.': return '>';
        case '/': return '?';
        default: return c;  // No shift mapping
    }
}

// Function declarations
void keyboard_init(void);

#endif
