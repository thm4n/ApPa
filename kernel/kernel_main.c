#include "../drivers/screen.h"
#include "idt.h"
#include "pic.h"
#include "../drivers/keyboard.h"
#include "kmalloc.h"

void __stack_chk_fail() {}

void main() {
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

	// Phase 3: Initialize kernel heap
	// Sets up the initial free block for dynamic memory allocation
	kmalloc_init();
	kprint("  [OK] Kernel heap initialized\n");

	// Phase 4: Initialize keyboard driver
	// Registers a handler for IRQ1 (keyboard interrupt)
	keyboard_init();
	kprint("  [OK] Keyboard initialized\n");

	// Phase 5: Enable interrupts globally
	// The STI (Set Interrupt Flag) instruction allows the CPU to
	// respond to hardware interrupts
	__asm__ volatile("sti");
	kprint("  [OK] Interrupts enabled\n\n");

	kprint("System ready. Type something!\n");
	kprint("> ");

	// Infinite loop - CPU halts until interrupt occurs
	// When you press a key:
	//   1. Keyboard sends signal to PIC (IRQ1)
	//   2. PIC signals CPU
	//   3. CPU jumps to our keyboard handler (INT 33)
	//   4. Handler reads key, displays it, sends EOI
	//   5. CPU returns here and halts again
	while (1) {
		__asm__ volatile("hlt");
	}
}
