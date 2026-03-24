/**
 * timer.c - System timer driver
 * 
 * Handles IRQ0 timer interrupts from the PIT (Programmable Interval Timer).
 * Tracks system uptime and provides timing functions.
 */

#include "timer.h"
#include "isr.h"
#include "../libc/string.h"

// Timer configuration
#define TIMER_FREQUENCY 100  // Hz (ticks per second)

// Timer tick counter (incremented by IRQ0 handler)
static volatile uint32_t timer_ticks = 0;

/**
 * timer_handler - IRQ0 interrupt handler
 * @regs: CPU register state at time of interrupt
 * 
 * Called automatically by the PIT at the configured frequency.
 * Updates tick counter and displays a visual indicator on screen.
 */
static void timer_handler(registers_t *regs) {
	// Suppress unused parameter warning
	(void)regs;
	
	timer_ticks++;
}

/**
 * timer_init - Initialize the timer subsystem
 * 
 * Registers the timer interrupt handler for IRQ0 (interrupt 32 after PIC remap).
 */
void timer_init(void) {
	timer_ticks = 0;
	register_interrupt_handler(32, timer_handler);
}

/**
 * get_timer_ticks - Get raw timer tick count
 * 
 * Returns: Number of timer ticks since boot
 */
uint32_t get_timer_ticks(void) {
	return timer_ticks;
}

/**
 * get_uptime_seconds - Get system uptime in seconds
 * 
 * Returns: Number of seconds since boot
 */
uint32_t get_uptime_seconds(void) {
	return timer_ticks / TIMER_FREQUENCY;
}

/**
 * get_uptime_string - Format uptime as human-readable string
 * @buffer: Output buffer (must be at least 32 bytes)
 * 
 * Formats uptime as:
 *   - "Xs" for less than 60 seconds
 *   - "Xm Ys" for less than 60 minutes
 *   - "Xh Ym" for 60 minutes and above
 */
void get_uptime_string(char* buffer) {
	uint32_t total_seconds = get_uptime_seconds();
	
	if (total_seconds < 60) {
		// Less than 1 minute: show seconds only
		uint32_t s = total_seconds;
		uitoa(s, buffer, 10);
		strcat(buffer, "s");
	} else if (total_seconds < 3600) {
		// Less than 1 hour: show minutes and seconds
		uint32_t m = total_seconds / 60;
		uint32_t s = total_seconds % 60;
		
		char temp[16];
		uitoa(m, buffer, 10);
		strcat(buffer, "m ");
		uitoa(s, temp, 10);
		strcat(buffer, temp);
		strcat(buffer, "s");
	} else {
		// 1 hour or more: show hours and minutes
		uint32_t h = total_seconds / 3600;
		uint32_t m = (total_seconds % 3600) / 60;
		
		char temp[16];
		uitoa(h, buffer, 10);
		strcat(buffer, "h ");
		uitoa(m, temp, 10);
		strcat(buffer, temp);
		strcat(buffer, "m");
	}
}
