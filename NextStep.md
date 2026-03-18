# IRQ Setup Plan

## Phase 1: CPU Exception Handlers (ISRs 0-31)
These handle CPU exceptions like divide-by-zero, page faults, etc.

| Task | File | Description |
|------|------|-------------|
| 1.1 | `kernel/idt.h` | Define IDT entry struct (8 bytes: offset_low, selector, zero, flags, offset_high) |
| 1.2 | `kernel/idt.c` | Create IDT array (256 entries) and `idt_set_gate()` function |
| 1.3 | `kernel/idt.c` | Write `idt_load()` using `lidt` instruction |
| 1.4 | `kernel/isr.asm` | Write 32 ISR stubs for CPU exceptions (push error code, push ISR number, jump to common handler) |
| 1.5 | `kernel/isr.c` | Write `isr_handler()` — common C handler that prints exception info |

## Phase 2: PIC Remapping
The 8259 PIC maps IRQ0-7 to interrupts 8-15 by default, conflicting with CPU exceptions.

| Task | File | Description |
|------|------|-------------|
| 2.1 | `kernel/pic.h` | Define PIC ports (0x20, 0x21, 0xA0, 0xA1) and ICW constants |
| 2.2 | `kernel/pic.c` | Write `pic_remap()` — remap IRQ0-7 → INT 32-39, IRQ8-15 → INT 40-47 |
| 2.3 | `kernel/pic.c` | Write `pic_send_eoi()` — send End-Of-Interrupt to PIC |

## Phase 3: IRQ Handlers (ISRs 32-47)
Hardware interrupts: timer, keyboard, etc.

| Task | File | Description |
|------|------|-------------|
| 3.1 | `kernel/isr.asm` | Write 16 IRQ stubs (IRQ0-15 → call common IRQ handler) |
| 3.2 | `kernel/isr.c` | Write `irq_handler()` — dispatch to registered handlers, send EOI |
| 3.3 | `kernel/isr.c` | Write `register_interrupt_handler()` — callback registration |

## Phase 4: Keyboard Driver
IRQ1 = keyboard interrupt.

| Task | File | Description |
|------|------|-------------|
| 4.1 | `drivers/keyboard.h` | Define scancode-to-ASCII table |
| 4.2 | `drivers/keyboard.c` | Write `keyboard_handler()` — read port 0x60, translate scancode, print char |
| 4.3 | `drivers/keyboard.c` | Write `keyboard_init()` — register handler for IRQ1 |

## Phase 5: Integration

| Task | File | Description |
|------|------|-------------|
| 5.1 | `kernel/kernel_main.c` | Call `idt_init()`, `pic_remap()`, `keyboard_init()` |
| 5.2 | `kernel/kernel_main.c` | Enable interrupts with `asm volatile("sti")` |
| 5.3 | `makefile` | Add new `.c` and `.asm` files to build |

---

## File Structure After Completion
```
kernel/
    idt.c / idt.h       # IDT setup
    isr.asm             # ISR/IRQ assembly stubs
    isr.c / isr.h       # C handlers + dispatcher
    pic.c / pic.h       # PIC remapping
    kernel_main.c       # Entry point
drivers/
    keyboard.c / keyboard.h
    ports.c / ports.h   # Already exists
    screen.c / screen.h # Already exists
```

## Execution Order at Runtime
```
main()
  → idt_init()        # Build IDT, load with lidt
  → pic_remap()       # Move IRQs to INT 32-47
  → keyboard_init()   # Register IRQ1 handler
  → sti               # Enable interrupts
  → [keyboard works]
```
