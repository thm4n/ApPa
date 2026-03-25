#include "../../drivers/screen.h"
#include "../../drivers/serial.h"
#include "../arch/gdt.h"
#include "../arch/idt.h"
#include "../arch/pic.h"
#include "../arch/pit.h"
#include "../arch/tss.h"
#include "timer.h"
#include "../mem/pmm.h"
#include "../mem/paging.h"
#include "../../drivers/ata.h"
#include "../../drivers/ata_blockdev.h"
#include "../../fs/ramdisk.h"
#include "../../fs/simplefs.h"
#include "../../drivers/keyboard.h"
#include "../mem/kmalloc.h"
#include "klog.h"
#include "../task/task.h"
#include "../task/sched.h"
#include "../arch/syscall.h"
#include "../../shell/shell.h"
#include "../../tests/tests.h"

void __stack_chk_fail() {}

void main() {
	// Initialize serial port for -nographic mode
	serial_init();
	
	// Clear screen and print welcome message
	clear_screen();
	kprint("ApPa Kernel v0.1\n");
	kprint("Initializing system...\n");

	// Phase 0: Set up Global Descriptor Table (GDT)
	// Replace bootloader's temporary GDT with kernel's permanent GDT
	gdt_init();
	kprint("  [OK] GDT initialized\n");

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
	
	// Phase 3: Initialize PIT (Programmable Interval Timer) hardware
	// Configure PIT to generate IRQ0 at 100Hz (10ms intervals)
	pit_init(100);
	kprint("  [OK] PIT initialized (100Hz)\n");
	
	// Phase 3.5: Initialize timer subsystem
	// Registers IRQ0 handler and sets up tick counter
	timer_init();
	kprint("  [OK] Timer initialized\n");

	// Phase 4: Initialize kernel heap
	// Sets up the initial free block for dynamic memory allocation
	kmalloc_init();
	kprint("  [OK] Kernel heap initialized\n");

	// Phase 4.25: Initialize physical memory manager
	// Sets up bitmap allocator for 4KB physical page frames
	pmm_init();
	kprint("  [OK] Physical memory manager initialized\n");

	// Phase 4.5: Enable paging with identity mapping
	// Sets up page directory + 4 page tables covering 0-16MB
	paging_init();
	kprint("  [OK] Paging enabled (identity map 0-16MB)\n");

	// Phase 4.6: Initialize Task State Segment
	// Installs TSS in GDT entry 5 so the CPU knows where the
	// kernel stack is during privilege transitions and context switches
	tss_init(0x10, 0x9FC00);
	kprint("  [OK] TSS initialized\n");

	// Phase 4.75: Initialize kernel logging system
	// Sets up circular buffer for persistent kernel logs
	klog_init();
	kprint("  [OK] Kernel logging initialized\n");
	klog_info("ApPa Kernel v0.1 booting...");

	// Phase 4.8: Initialize ATA disk driver
	// Detect primary master (boot image) and primary slave (data disk)
	ata_init();
	const ata_drive_info_t* master_disk = ata_get_info();
	if (master_disk->present) {
		kprint("  [OK] ATA master: ");
		kprint((char*)master_disk->model);
		kprint("\n");
	} else {
		kprint("  [--] No ATA master detected\n");
	}
	const ata_drive_info_t* slave_disk = ata_get_slave_info();
	if (slave_disk->present) {
		kprint("  [OK] ATA slave:  ");
		kprint((char*)slave_disk->model);
		kprint("\n");
	} else {
		kprint("  [--] No ATA slave detected\n");
	}

	// Phase 14: Initialize filesystem on ATA slave (persistent) with
	// ramdisk fallback when no slave disk is present (e.g. no -hdb)
	block_device_t* blkdev = ata_blockdev_init();
	if (blkdev) {
		kprint("  [OK] ATA block device initialized (persistent)\n");
		klog_info("FS: using ATA slave (persistent)");
	} else {
		kprint("  [--] No ATA slave — falling back to RAM disk\n");
		blkdev = ramdisk_init(256);
		if (blkdev) {
			kprint("  [OK] RAM disk initialized (256 KB, volatile)\n");
			klog_info("FS: using RAM disk (volatile)");
		} else {
			kprint("  [FAIL] RAM disk init failed\n");
		}
	}
	if (blkdev) {
		if (fs_init(blkdev) == 0) {
			kprint("  [OK] SimpleFS mounted\n");
			klog_info("SimpleFS mounted");
		} else {
			kprint("  [FAIL] SimpleFS init failed\n");
		}
	}

	// Phase 5: Initialize keyboard driver
	// Registers a handler for IRQ1 (keyboard interrupt)
	keyboard_init();
	kprint("  [OK] Keyboard initialized\n");

	// Phase 6: Initialize command shell
	// Sets up command buffer and prepares for interactive input
	shell_init();
	kprint("  [OK] Shell initialized\n");

	// Phase 12: Initialize scheduler
	// Wraps the current execution context as the bootstrap/idle task
	sched_init();
	kprint("  [OK] Scheduler initialized\n");

	// Phase 13: Initialize system call interface
	// Registers INT 0x80 as a DPL=3 trap gate and populates the
	// syscall dispatch table (SYS_EXIT, SYS_WRITE, etc.)
	syscall_init();
	kprint("  [OK] Syscall interface initialized (INT 0x80)\n");

	// Phase 7: Enable interrupts globally
	// The STI (Set Interrupt Flag) instruction allows the CPU to
	// respond to hardware interrupts
	__asm__ volatile("sti");
	kprint("  [OK] Interrupts enabled\n\n");
	
	// Run unit tests before entering shell
	run_all_tests();
	
	// Phase 12: Enable preemptive scheduling
	// From this point, the timer ISR will call schedule() and
	// switch between tasks when time-slices expire
	sched_enable();
	kprint("  [OK] Preemptive scheduling enabled\n");

	kprint("=== ApPa OS Ready ===\n");
	kprint("Type 'help' for available commands\n\n");
	kprint("> ");

	// Idle loop — this is now the "idle" task (ID 0)
	// When no other task is READY, the scheduler keeps running this.
	// hlt suspends the CPU until the next interrupt, saving power.
	// task_reap() frees resources of any finished tasks.
	while (1) {
		__asm__ volatile("hlt");
		task_reap();
	}
}
