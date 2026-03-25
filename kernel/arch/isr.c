#include "isr.h"
#include "../../drivers/screen.h"

// Array of interrupt handlers (shared by ISR and IRQ)
isr_handler_t interrupt_handlers[256];

/*
 * CPU Exception Messages (Interrupts 0-31)
 * 
 * These are the 32 reserved interrupt vectors for CPU exceptions.
 * When the CPU detects an error condition, it triggers the corresponding
 * interrupt. Some exceptions push an error code onto the stack.
 */
static const char* exception_messages[] = {
    /* 0x00 - Division By Zero (#DE)
     * Triggered when dividing by zero or when the quotient is too large
     * to fit in the destination register. Occurs with DIV/IDIV instructions. */
    "Division By Zero",

    /* 0x01 - Debug (#DB)
     * Used for single-stepping, breakpoint conditions, and other debug events.
     * Can be a fault or trap depending on the condition. */
    "Debug",

    /* 0x02 - Non Maskable Interrupt (NMI)
     * Hardware interrupt that cannot be ignored. Typically used for critical
     * hardware failures like memory parity errors or watchdog timer expiration. */
    "Non Maskable Interrupt",

    /* 0x03 - Breakpoint (#BP)
     * Triggered by the INT3 instruction. Used by debuggers to set breakpoints.
     * This is a trap, so EIP points to the instruction after INT3. */
    "Breakpoint",

    /* 0x04 - Overflow (#OF)
     * Triggered by the INTO instruction when the overflow flag (OF) is set.
     * Indicates signed integer overflow occurred. */
    "Overflow",

    /* 0x05 - Bound Range Exceeded (#BR)
     * Triggered by the BOUND instruction when the array index is out of bounds.
     * Rarely used in modern code. */
    "Bound Range Exceeded",

    /* 0x06 - Invalid Opcode (#UD)
     * CPU encountered an undefined or reserved instruction, or an instruction
     * with invalid operands. Common when executing random data as code. */
    "Invalid Opcode",

    /* 0x07 - Device Not Available (#NM)
     * FPU/MMX/SSE instruction executed but no coprocessor present, or
     * task switch occurred with TS flag set (lazy FPU context switching). */
    "Device Not Available",

    /* 0x08 - Double Fault (#DF) [Error Code: Always 0]
     * Exception occurred while trying to handle a prior exception.
     * Usually indicates corrupted IDT, invalid stack, or kernel bug.
     * Often leads to a triple fault (reboot) if not handled. */
    "Double Fault",

    /* 0x09 - Coprocessor Segment Overrun (Legacy)
     * Only on 386/486. Triggered when FPU instruction caused a page/segment
     * fault. Modern CPUs use #GP or #PF instead. */
    "Coprocessor Segment Overrun",

    /* 0x0A - Invalid TSS (#TS) [Error Code: Selector index]
     * Task switch failed due to invalid Task State Segment.
     * Error code contains the selector index that caused the fault. */
    "Invalid TSS",

    /* 0x0B - Segment Not Present (#NP) [Error Code: Selector index]
     * Referenced segment descriptor has Present bit = 0.
     * Error code contains the selector of the non-present segment. */
    "Segment Not Present",

    /* 0x0C - Stack-Segment Fault (#SS) [Error Code: Selector or 0]
     * Stack operation exceeded segment limit, or stack segment not present.
     * Error code is selector if loading SS, otherwise 0. */
    "Stack-Segment Fault",

    /* 0x0D - General Protection Fault (#GP) [Error Code: Selector or 0]
     * Catch-all for protection violations: segment limit exceeded, write to
     * read-only segment, privileged instruction in user mode, etc.
     * Most common exception in kernel development. */
    "General Protection Fault",

    /* 0x0E - Page Fault (#PF) [Error Code: Flags]
     * Virtual memory violation. Error code bits indicate:
     *   Bit 0: 0=non-present page, 1=protection violation
     *   Bit 1: 0=read access, 1=write access
     *   Bit 2: 0=supervisor mode, 1=user mode
     * CR2 register contains the faulting linear address. */
    "Page Fault",

    /* 0x0F - Reserved
     * Intel reserved, should never occur. */
    "Reserved",

    /* 0x10 - x87 Floating-Point Exception (#MF)
     * Unmasked x87 FPU error: invalid operation, divide-by-zero, overflow,
     * underflow, precision loss, or denormalized operand. */
    "x87 Floating-Point Exception",

    /* 0x11 - Alignment Check (#AC) [Error Code: Always 0]
     * Unaligned memory access when alignment checking is enabled.
     * Only occurs in ring 3 when CR0.AM=1 and EFLAGS.AC=1. */
    "Alignment Check",

    /* 0x12 - Machine Check (#MC)
     * Internal CPU error or bus error detected. Model-specific, details in
     * MSRs. Usually indicates hardware failure. */
    "Machine Check",

    /* 0x13 - SIMD Floating-Point Exception (#XM/#XF)
     * Unmasked SSE/SSE2/SSE3 floating-point error.
     * Similar to #MF but for SIMD operations. */
    "SIMD Floating-Point Exception",

    /* 0x14 - Virtualization Exception (#VE)
     * EPT (Extended Page Table) violation in VMX guest.
     * Used by hypervisors for virtualization. */
    "Virtualization Exception",

    /* 0x15 - Control Protection Exception (#CP) [Error Code: Error type]
     * Control flow integrity violation. Triggered by CET (Control-flow
     * Enforcement Technology) when shadow stack or indirect branch
     * tracking detects tampering. */
    "Control Protection Exception",

    /* 0x16-0x1C - Reserved */
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",

    /* 0x1D - VMM Communication Exception (#VC) [Error Code: Info]
     * AMD SEV-ES: Guest needs to communicate with hypervisor.
     * Used in encrypted virtualization environments. */
    "VMM Communication Exception",

    /* 0x1E - Security Exception (#SX) [Error Code: Info]
     * AMD SEV-SNP: Security violation detected in encrypted VM.
     * Indicates potential attack or misconfiguration. */
    "Security Exception",

    /* 0x1F - Reserved */
    "Reserved"
};

// Register a handler for an interrupt
void register_interrupt_handler(uint8_t n, isr_handler_t handler) {
    interrupt_handlers[n] = handler;
}

// C handler called from assembly - handles CPU exceptions
void isr_handler(registers_t* regs) {
    // Check if a custom handler is registered
    if (interrupt_handlers[regs->int_no] != 0) {
        interrupt_handlers[regs->int_no](regs);
        return;
    }

    // Default handler: print exception info
    // Disable interrupts to prevent timer from corrupting output
    __asm__ volatile("cli");
    kprint("\n========== CPU EXCEPTION ==========\n");
    kprint("Exception: ");
    if (regs->int_no < 32) {
        kprint((char*)exception_messages[regs->int_no]);
    }
    kprint(" (");
    kprint_hex(regs->int_no);
    kprint(")\n");

    kprint("Error Code: ");
    kprint_hex(regs->err_code);
    kprint("\n");

    kprint("EIP: ");
    kprint_hex(regs->eip);
    kprint("  CS: ");
    kprint_hex(regs->cs);
    kprint("\n");

    kprint("EFLAGS: ");
    kprint_hex(regs->eflags);
    kprint("\n");

    kprint("EAX: ");
    kprint_hex(regs->eax);
    kprint("  EBX: ");
    kprint_hex(regs->ebx);
    kprint("\n");

    kprint("ECX: ");
    kprint_hex(regs->ecx);
    kprint("  EDX: ");
    kprint_hex(regs->edx);
    kprint("\n");

    kprint("ESP: ");
    kprint_hex(regs->esp);
    kprint("  EBP: ");
    kprint_hex(regs->ebp);
    kprint("\n");

    kprint("ESI: ");
    kprint_hex(regs->esi);
    kprint("  EDI: ");
    kprint_hex(regs->edi);
    kprint("\n");

    kprint("DS: ");
    kprint_hex(regs->ds);
    kprint("\n");

    kprint("====================================\n");
    kprint("System Halted.\n");

    // Halt the system
    for (;;) {
        __asm__ volatile("hlt");
    }
}
