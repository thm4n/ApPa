#ifndef TIMER_H
#define TIMER_H

#include "../../libc/stdint.h"

/**
 * timer_init - Initialize the timer subsystem
 * 
 * Registers the timer interrupt handler for IRQ0.
 * Must be called after idt_init() and pic_remap().
 */
void timer_init(void);

/**
 * get_timer_ticks - Get raw timer tick count
 * 
 * Returns: Number of timer ticks since boot (at configured frequency)
 */
uint32_t get_timer_ticks(void);

/**
 * get_uptime_seconds - Get system uptime in seconds
 * 
 * Returns: Number of seconds since boot
 */
uint32_t get_uptime_seconds(void);

/**
 * get_uptime_string - Format uptime as human-readable string
 * @buffer: Output buffer (must be at least 32 bytes)
 * 
 * Formats uptime as "XXm YYs" or "XXh YYm" for longer uptimes.
 */
void get_uptime_string(char* buffer);

#endif // TIMER_H
