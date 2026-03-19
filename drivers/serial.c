#include "serial.h"
#include "ports.h"

// Serial port register offsets
#define SERIAL_DATA_REG        0  // Data register (read/write)
#define SERIAL_INT_ENABLE_REG  1  // Interrupt enable
#define SERIAL_FIFO_CTRL_REG   2  // FIFO control
#define SERIAL_LINE_CTRL_REG   3  // Line control
#define SERIAL_MODEM_CTRL_REG  4  // Modem control
#define SERIAL_LINE_STATUS_REG 5  // Line status

void serial_init() {
    // Disable interrupts
    port_byte_out(SERIAL_COM1 + SERIAL_INT_ENABLE_REG, 0x00);
    
    // Enable DLAB (set baud rate divisor)
    port_byte_out(SERIAL_COM1 + SERIAL_LINE_CTRL_REG, 0x80);
    
    // Set divisor to 3 (38400 baud)
    port_byte_out(SERIAL_COM1 + SERIAL_DATA_REG, 0x03);
    port_byte_out(SERIAL_COM1 + SERIAL_INT_ENABLE_REG, 0x00);
    
    // 8 bits, no parity, one stop bit
    port_byte_out(SERIAL_COM1 + SERIAL_LINE_CTRL_REG, 0x03);
    
    // Enable FIFO, clear them, with 14-byte threshold
    port_byte_out(SERIAL_COM1 + SERIAL_FIFO_CTRL_REG, 0xC7);
    
    // IRQs enabled, RTS/DSR set
    port_byte_out(SERIAL_COM1 + SERIAL_MODEM_CTRL_REG, 0x0B);
}

static int is_transmit_empty() {
    return port_byte_in(SERIAL_COM1 + SERIAL_LINE_STATUS_REG) & 0x20;
}

void serial_putc(char c) {
    // Wait for transmit buffer to be empty
    while (!is_transmit_empty());
    
    // Send character
    port_byte_out(SERIAL_COM1 + SERIAL_DATA_REG, c);
}

void serial_puts(const char* str) {
    for (int i = 0; str[i] != '\0'; i++) {
        // Convert \n to \r\n for proper terminal display
        if (str[i] == '\n') {
            serial_putc('\r');
        }
        serial_putc(str[i]);
    }
}
