#include "../drivers/screen.h"
#include "../drivers/serial.h"
#include "idt.h"
#include "pic.h"
#include "pit.h"
#include "../drivers/keyboard.h"
#include "kmalloc.h"
#include "klog.h"
#include "shell.h"
#include "../tests/tests.h"

void __stack_chk_fail() {}

void main() {
	// Initialize serial port for -nographic mode
	serial_init();
	
	// Clear screen and print welcome message
	clear_screen();
	kprint("ApPa Kernel v0.1\n");
	kprint("Initializing interrupt system...\n");

	// Phase 1: Set up Interrupt Descriptor Table (IDT)
	// This configures the CPU's interrupt handling mechanism
	// and registers handlers for all 256 possible interrupts
	idt_init();
	kprint("  [OK] IDT initialized\n");

	// Phase 2: Remap the Programmable Interrupt Controller (PIC)
	// By default, IRQs 0-7 conflict with CPU exceptions 8-15
	// Remap IRQ 0-7 to interrupts 32-39, IRQ 8-15 to interrupts 40-47
	pic_remap(32, 40);
	kprint("  [OK] PIC remapped\n");
	
	// Initialize PIT (Programmable Interval Timer) for timer interrupts
	// This will generate IRQ0 at 100Hz (10ms intervals)
	pit_init(100);
	kprint("  [OK] PIT initialized (100Hz)\n");
	
	// Initialize PIT (timer) to generate IRQ0 at 100Hz (10ms intervals)
	pit_init(100);
	kprint("  [OK] PIT initialized (100Hz)\n");

	// Phase 3: Initialize kernel heap
	// Sets up the initial free block for dynamic memory allocation
	kmalloc_init();
	kprint("  [OK] Kernel heap initialized\n");

	// Phase 3.5: Initialize kernel logging system
	// Sets up circular buffer for persistent kernel logs
	//klog_init();  // DISABLED FOR DEBUGGING
	kprint("  [OK] Kernel logging initialized (DISABLED FOR DEBUG)\n");
	//klog_info("ApPa Kernel v0.1 booting...");
	//klog_info("Memory, IDT, and PIC configured");

	// Phase 4: Initialize keyboard driver
	// Registers a handler for IRQ1 (keyboard interrupt)
	keyboard_init();
	kprint("  [OK] Keyboard initialized\n");

	// Phase 5: Initialize command shell
	// Sets up command buffer and prepares for interactive input
	shell_init();
	kprint("  [OK] Shell initialized\n");

	// Phase 6: Enable interrupts globally
	// The STI (Set Interrupt Flag) instruction allows the CPU to
	// respond to hardware interrupts
	__asm__ volatile("sti");
	kprint("  [OK] Interrupts enabled\n\n");
	
	kprint("=== ApPa OS Ready ===\n");
	kprint("Timer ticking in top-right corner (0-9 = IRQ0 working)\n");
	kprint("Type 'help' for available commands\n\n");
	kprint("> ");

	// Infinite loop - DON'T use hlt for now (testing)
	// When you press a key:
	//   1. Keyboard sends signal to PIC (IRQ1)
	//   2. PIC signals CPU
	//   3. CPU jumps to our keyboard handler (INT 33)
	//   4. Handler reads key, displays it, sends EOI
	//   5. CPU returns here and halts again
	while (1) {
		// Just loop (no hlt) to test if hlt is the problem
	}
}
