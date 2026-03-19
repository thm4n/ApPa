/**
 * pit.c - Programmable Interval Timer (8253/8254 PIT) driver
 * 
 * The PIT generates periodic timer interrupts (IRQ0) used for task scheduling,
 * timekeeping, and other time-critical operations.
 */

#include "pit.h"
#include "../drivers/ports.h"

#define PIT_CHANNEL0_DATA  0x40
#define PIT_COMMAND        0x43

// Command bits:
// Bits 6-7: Channel select (00 = channel 0)
// Bits 4-5: Access mode (11 = lobyte/hibyte)  
// Bits 1-3: Operating mode (011 = square wave generator)
// Bit 0: BCD/Binary mode (0 = 16-bit binary)
#define PIT_CMD_BINARY     0x00  // Use binary mode
#define PIT_CMD_MODE3      0x06  // Mode 3: Square wave generator
#define PIT_CMD_RW_BOTH    0x30  // Read/Write lobyte then hibyte
#define PIT_CMD_CHANNEL0   0x00  // Select channel 0

/**
 * pit_init - Initialize the Programmable Interval Timer
 * @frequency: Desired interrupt frequency in Hz
 * 
 * The PIT base frequency is 1.193182 MHz. We divide this by the desired
 * frequency to get the divisor value.
 * 
 * Common frequencies:
 *   100 Hz = 10 ms interval (good for scheduling)
 *   1000 Hz = 1 ms interval (higher resolution)
 *   18.2 Hz = original IBM PC timer frequency
 */
void pit_init(uint32_t frequency) {
    // Calculate divisor: PIT_BASE_FREQ / desired_frequency
    // PIT base frequency is 1193182 Hz (1.193182 MHz)
    uint32_t divisor = 1193182 / frequency;
    
    // Ensure divisor fits in 16 bits
    if (divisor > 0xFFFF) {
        divisor = 0xFFFF;
    }
    if (divisor < 1) {
        divisor = 1;
    }
    
    // Send command byte: channel 0, lobyte/hibyte, mode 3, binary
    uint8_t command = PIT_CMD_CHANNEL0 | PIT_CMD_RW_BOTH | PIT_CMD_MODE3 | PIT_CMD_BINARY;
    port_byte_out(PIT_COMMAND, command);
    
    // Send divisor (low byte first, then high byte)
    port_byte_out(PIT_CHANNEL0_DATA, (uint8_t)(divisor & 0xFF));
    port_byte_out(PIT_CHANNEL0_DATA, (uint8_t)((divisor >> 8) & 0xFF));
}
