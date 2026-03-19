/**
 * pit.h - Programmable Interval Timer header
 */

#ifndef PIT_H
#define PIT_H

#include "../libc/stdint.h"

/**
 * pit_init - Initialize the PIT to generate periodic IRQ0 interrupts
 * @frequency: Desired frequency in Hz (e.g., 100 for 100Hz = 10ms intervals)
 */
void pit_init(uint32_t frequency);

#endif /* PIT_H */
