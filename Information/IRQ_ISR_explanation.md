# IRQ Educational Guide: Understanding Interrupts in x86

## Table of Contents
1. [What are Interrupts?](#what-are-interrupts)
2. [Phase 1: CPU Exception Handlers](#phase-1-cpu-exception-handlers)
3. [Phase 2: PIC Remapping](#phase-2-pic-remapping)
4. [Phase 3: IRQ Handlers](#phase-3-irq-handlers)
5. [Phase 4: Keyboard Driver](#phase-4-keyboard-driver)
6. [Phase 5: Integration](#phase-5-integration)

---

## What are Interrupts?

### The Big Picture

Imagine you're reading a book and someone taps you on the shoulder. You:
1. **Stop reading** and remember your page number
2. **Turn around** to see what they want
3. **Help them** with their request
4. **Return to your book** at the exact page you left off

This is exactly how interrupts work in a CPU!

### Types of Interrupts

**1. Hardware Interrupts (IRQs - Interrupt ReQuests)**
- External devices signal the CPU for attention
- Examples: keyboard key pressed, timer tick, disk operation complete
- Asynchronous (happen at unpredictable times)

**2. Software Interrupts (Exceptions)**
- CPU detects error conditions during instruction execution
- Examples: division by zero, invalid memory access, page fault
- Synchronous (happen at specific instruction points)

**3. System Calls (INT instruction)**
- Programs intentionally trigger interrupts to request OS services
- Example: `INT 0x80` on Linux (legacy) for system calls

### Why Do We Need Interrupts?

**Without interrupts (Polling):**
```c
while (1) {
    if (keyboard_has_data()) handle_keyboard();
    if (mouse_has_data()) handle_mouse();
    if (disk_ready()) handle_disk();
    // CPU wastes time constantly checking!
}
```

**With interrupts:**
```c
// CPU does useful work...
// Device sends IRQ → CPU automatically handles it → CPU returns to work
// No wasted CPU cycles!
```

---

## Phase 1: CPU Exception Handlers (ISRs 0-31) ✅ COMPLETED

### Overview
Phase 1 sets up the foundation: the Interrupt Descriptor Table (IDT) and handlers for CPU exceptions (errors that occur during execution).

---

### Step 1.1: Define IDT Entry Struct

**What it does:**
Defines the structure of a single entry in the Interrupt Descriptor Table.

**How it works:**
```c
struct InterruptDescriptor32 {
   uint16_t offset_1;        // Bits 0-15 of handler address
   uint16_t selector;        // Code segment selector (where handler lives)
   uint8_t  zero;            // Reserved, always 0
   uint8_t  type_attributes; // Gate type, DPL, Present bit
   uint16_t offset_2;        // Bits 16-31 of handler address
};
```

**Why this structure?**

The x86 CPU requires this exact 8-byte format for each IDT entry. When an interrupt occurs:

1. **CPU reads the IDT entry** corresponding to the interrupt number
2. **Extracts the handler address**: `(offset_2 << 16) | offset_1`
3. **Checks the selector**: Which segment contains the handler code?
4. **Verifies permissions**: Can this privilege level handle this interrupt?
5. **Jumps to the handler**: Executes your code

**Why split the address into two parts?**
Historical reasons from 16-bit x86 architecture. The format was extended for 32-bit but kept the split to maintain backward compatibility.

**Type/Attributes byte breakdown:**
```
Bit 7:     Present (1 = valid entry, 0 = unused)
Bits 6-5:  DPL (Descriptor Privilege Level): Who can call this?
           00 = Ring 0 (kernel only)
           11 = Ring 3 (user code can trigger via INT)
Bit 4:     Storage segment (0 for interrupt gates)
Bits 3-0:  Gate type:
           0xE = 32-bit Interrupt Gate (disables interrupts)
           0xF = 32-bit Trap Gate (keeps interrupts enabled)
```

Typical value: `0x8E` = `1000 1110` binary
- Present (1)
- DPL 00 (Ring 0 only)
- 32-bit Interrupt Gate (0xE)

---

### Step 1.2: Create IDT Array and idt_set_gate() Function

**What it does:**
- Creates an array of 256 IDT entries (one for each possible interrupt 0-255)
- Provides a function to populate each entry

**How it works:**

```c
struct InterruptDescriptor32 idt[256];  // The table itself

void idt_set_gate(uint8_t num, uint32_t handler, 
                  uint16_t selector, uint8_t flags) {
    idt[num].offset_1 = handler & 0xFFFF;         // Low 16 bits
    idt[num].offset_2 = (handler >> 16) & 0xFFFF; // High 16 bits
    idt[num].selector = selector;                  // 0x08 = kernel code segment
    idt[num].zero = 0;                             
    idt[num].type_attributes = flags;              // 0x8E typically
}
```

**Why 256 entries?**
The x86 interrupt vector is 8 bits (0-255). The CPU uses the interrupt number as an index into this table:

- **0-31**: Reserved for CPU exceptions (divide by zero, page fault, etc.)
- **32-47**: Typically mapped to hardware IRQs (keyboard, timer, etc.)
- **48-255**: Available for software interrupts and custom use

**Why do we need selector (0x08)?**
When an interrupt occurs, the CPU needs to know which segment contains the handler code. In protected mode:
- `0x08` points to the kernel code segment in the GDT (Global Descriptor Table)
- This was set up during boot in your `32bit-gdt.asm`
- It ensures interrupt handlers run with kernel privileges

---

### Step 1.3: Write idt_load() Using lidt Instruction

**What it does:**
Loads the IDT into the CPU using the special `lidt` instruction.

**How it works:**

First, we create a descriptor that tells the CPU where the IDT is:
```c
struct idt_ptr {
    uint16_t limit;  // Size of IDT in bytes - 1
    uint32_t base;   // Memory address of IDT
} __attribute__((packed));

idtp.limit = (sizeof(struct InterruptDescriptor32) * 256) - 1;  // 2047 bytes
idtp.base = (uint32_t)&idt;  // Address of our idt array
```

Then in assembly:
```asm
idt_load:
    mov eax, [esp + 4]  ; Get pointer to idt_ptr struct
    lidt [eax]          ; Load IDT register
    ret
```

**Why this works:**

The CPU has a special register called **IDTR** (IDT Register) that stores:
- **Base**: Where the IDT starts in memory
- **Limit**: How big it is

The `lidt` instruction loads this register. After this:
1. When an interrupt occurs, the CPU reads IDTR
2. Calculates: `IDTR.base + (interrupt_number * 8)` to find the entry
3. Jumps to the handler address in that entry

**Why `__attribute__((packed))`?**
Prevents the compiler from adding padding bytes between `limit` and `base`. The CPU expects this exact 6-byte structure:
```
Bytes 0-1: limit (16 bits)
Bytes 2-5: base (32 bits)
```

---

### Step 1.4: Write 32 ISR Stubs for CPU Exceptions

**What it does:**
Creates 32 assembly stubs (one for each CPU exception) that prepare the stack and jump to a common handler.

**How it works:**

The challenge: CPU exceptions have inconsistent behavior:
- Some push an error code (e.g., Page Fault, General Protection Fault)
- Some don't (e.g., Division by Zero, Invalid Opcode)

We need a **consistent stack layout** for our C handler:

```
[Error Code]  ← We ensure this is always present
[IRQ Number]  ← We push this ourselves
[CPU state]   ← Saved by our stub
```

**Solution: Use macros**

```asm
; For exceptions that DON'T push error code
%macro ISR_NOERR 1
global isr%1
isr%1:
    push dword 0        ; Push DUMMY error code (keep stack consistent)
    push dword %1       ; Push interrupt number
    jmp isr_common_stub
%endmacro

; For exceptions that DO push error code
%macro ISR_ERR 1
global isr%1
isr%1:
    ; CPU already pushed error code
    push dword %1       ; Push interrupt number
    jmp isr_common_stub
%endmacro

ISR_NOERR 0   ; Divide by Zero - no error code
ISR_ERR   8   ; Double Fault - has error code
ISR_ERR   14  ; Page Fault - has error code
```

**What isr_common_stub does:**

```asm
isr_common_stub:
    pusha              ; Save all registers (EAX, ECX, EDX, EBX, ESP, EBP, ESI, EDI)
    
    mov ax, ds         
    push eax           ; Save data segment
    
    mov ax, 0x10       ; Load kernel data segment
    mov ds, ax         ; Now we can access kernel data
    mov es, ax
    mov fs, ax
    mov gs, ax
    
    push esp           ; Push pointer to register structure
    call isr_handler   ; Call C handler
    
    add esp, 4         ; Clean up parameter
    
    pop eax            ; Restore data segment
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    
    popa               ; Restore all registers
    add esp, 8         ; Remove error code and IRQ number
    iret               ; Return from interrupt
```

**Why save and restore everything?**

When an interrupt occurs:
1. **CPU automatically saves**: EIP, CS, EFLAGS (to know where to return)
2. **We must save**: All general-purpose registers, segment registers
3. **Why?** The interrupted code expects to resume with identical CPU state
4. **Otherwise**: Variables in registers would be corrupted!

**Why switch to kernel data segment (0x10)?**

The interrupted code might have been in user mode with user data segment. Our kernel handler needs to access kernel data structures, so we temporarily switch segments.

**Why iret instead of ret?**

`iret` (Interrupt Return) is special:
- Pops EIP (where to return)
- Pops CS (code segment)  
- Pops EFLAGS (restores interrupt flag and other state)
- If switching privilege levels, also pops ESP and SS

Regular `ret` only pops EIP, which isn't enough.

---

### Step 1.5: Write isr_handler() - Common C Handler

**What it does:**
Receives register state from assembly, prints detailed exception information, and halts the system.

**How it works:**

```c
void isr_handler(registers_t* regs) {
    // Check if custom handler registered
    if (interrupt_handlers[regs->int_no] != 0) {
        interrupt_handlers[regs->int_no](regs);
        return;
    }
    
    // Default: print exception and halt
    kprint("Exception: ");
    kprint(exception_messages[regs->int_no]);
    kprint_hex(regs->eip);  // Where did it crash?
    kprint_hex(regs->err_code);  // Why did it crash?
    
    for (;;) asm("hlt");  // Stop the CPU
}
```

**Why the halt loop?**

After a CPU exception, the system is in an **undefined state**:
- Memory might be corrupted
- Execution can't safely continue
- Returning from the interrupt would likely cause another exception

The `hlt` instruction:
- Stops the CPU until next interrupt
- Since interrupts are disabled (interrupt gate), CPU stops forever
- Prevents cascade of exceptions (triple fault → reboot)

**Why print register values?**

Debugging information:
- **EIP**: Where was the CPU when it crashed?
- **Error Code**: Additional info (e.g., which segment caused fault)
- **Registers**: What values was the code working with?

Example: Page Fault (exception 14)
- EIP tells you which instruction tried to access memory
- Error code bits tell you: read or write? user or kernel? page present?
- CR2 register (not shown here) contains the address that was accessed

---

## Phase 2: PIC Remapping

### Overview
The 8259 Programmable Interrupt Controller (PIC) manages hardware interrupts (IRQs). We must remap it to avoid conflicts with CPU exceptions.

---

### The Problem: IRQ/Exception Conflict

**Default PIC mapping (IBM PC compatible):**
- Master PIC: IRQ 0-7 → Interrupts 8-15
- Slave PIC: IRQ 8-15 → Interrupts 0x70-0x77

**Why this is bad:**
```
IRQ 0 (Timer)      → INT 8  (Double Fault exception!)
IRQ 1 (Keyboard)   → INT 9  (Coprocessor Segment Overrun)
IRQ 6 (Floppy)     → INT 14 (Page Fault!)
```

When a keyboard key is pressed, the CPU thinks it's a coprocessor error! This is a disaster.

**Solution: Remap IRQs to 32-47:**
```
IRQ 0-7   → INT 32-39  (After CPU exceptions 0-31)
IRQ 8-15  → INT 40-47
```

---

### Understanding the 8259 PIC

**Hardware setup:**
```
        [Master PIC]                [Slave PIC]
        IRQ 0 - Timer               IRQ  8 - RTC
        IRQ 1 - Keyboard            IRQ  9 - ACPI
        IRQ 2 - Cascade (Slave) ←→  IRQ 10 - Available
        IRQ 3 - COM2                IRQ 11 - Available
        IRQ 4 - COM1                IRQ 12 - PS/2 Mouse
        IRQ 5 - LPT2/Sound          IRQ 13 - FPU
        IRQ 6 - Floppy              IRQ 14 - Primary ATA
        IRQ 7 - LPT1                IRQ 15 - Secondary ATA
```

**How it works:**
1. Device sends signal to PIC
2. PIC sets corresponding bit in IRR (Interrupt Request Register)
3. If interrupt not masked, PIC signals CPU via INTR pin
4. CPU acknowledges, PIC sends interrupt number
5. CPU jumps to IDT entry
6. Handler sends EOI (End Of Interrupt) to PIC
7. PIC clears ISR bit, ready for next interrupt

---

### Step 2.1: Define PIC Ports and Constants

**What it does:**
Defines I/O ports and command values for programming the PIC.

**The ports:**
```c
#define PIC1_COMMAND    0x20    // Master PIC command port
#define PIC1_DATA       0x21    // Master PIC data port
#define PIC2_COMMAND    0xA0    // Slave PIC command port
#define PIC2_DATA       0xA1    // Slave PIC data port
```

**Why these specific addresses?**
Hardware design from the original IBM PC (1981). These ports are hardwired into the motherboard chipset. When you write to port 0x20, it physically sends data to the PIC chip.

**Initialization Command Words (ICWs):**
```c
#define ICW1_INIT       0x10    // Initialization command
#define ICW1_ICW4       0x01    // ICW4 will be sent
#define ICW4_8086       0x01    // 8086 mode (not 8080 mode)
```

**Why ICWs?**
The PIC requires a specific initialization sequence:
1. **ICW1**: "I'm going to configure you, expect 3-4 more commands"
2. **ICW2**: "Map your 8 interrupts starting at this number"
3. **ICW3**: "You're master/slave, here's cascade info"
4. **ICW4**: "Use 8086 mode, auto EOI or not, etc."

This protocol dates back to the Intel 8259 chip design from 1976!

---

### Step 2.2: Write pic_remap()

**What it does:**
Reconfigures both PICs to map IRQs 0-15 to interrupts 32-47.

**How it works:**

```c
void pic_remap(uint8_t offset1, uint8_t offset2) {
    // Save current masks
    uint8_t mask1 = inb(PIC1_DATA);
    uint8_t mask2 = inb(PIC2_DATA);
    
    // Start initialization sequence
    outb(PIC1_COMMAND, ICW1_INIT | ICW1_ICW4);
    io_wait();
    outb(PIC2_COMMAND, ICW1_INIT | ICW1_ICW4);
    io_wait();
    
    // ICW2: Set vector offsets
    outb(PIC1_DATA, offset1);  // Master: IRQ 0-7 → INT 32-39
    io_wait();
    outb(PIC2_DATA, offset2);  // Slave: IRQ 8-15 → INT 40-47
    io_wait();
    
    // ICW3: Configure cascade
    outb(PIC1_DATA, 0x04);     // Master: Slave on IRQ2
    io_wait();
    outb(PIC2_DATA, 0x02);     // Slave: Cascade identity 2
    io_wait();
    
    // ICW4: Set 8086 mode
    outb(PIC1_DATA, ICW4_8086);
    io_wait();
    outb(PIC2_DATA, ICW4_8086);
    io_wait();
    
    // Restore masks
    outb(PIC1_DATA, mask1);
    outb(PIC2_DATA, mask2);
}
```

**Why save and restore masks?**
The interrupt mask determines which IRQs are enabled. We don't want to accidentally enable/disable devices during reconfiguration.

**Why io_wait()?**
Old hardware is slow! The PIC needs time to process each command. Typical implementation:
```c
void io_wait() {
    outb(0x80, 0);  // Write to unused port
}
```
Writing to port 0x80 takes a few CPU cycles, giving the PIC time to settle.

**ICW3 cascade explanation:**
- Master's `0x04` = binary `0000 0100` = "Slave is connected to IRQ 2"
- Slave's `0x02` = "I am cascade identity 2"
- When IRQ 2 triggers, master knows to check slave PIC

---

### Step 2.3: Write pic_send_eoi()

**What it does:**
Sends "End Of Interrupt" signal to tell the PIC we've finished handling the interrupt.

**How it works:**

```c
void pic_send_eoi(uint8_t irq) {
    if (irq >= 8) {
        outb(PIC2_COMMAND, 0x20);  // Send EOI to slave
    }
    outb(PIC1_COMMAND, 0x20);      // Send EOI to master
}
```

**Why is this necessary?**

The PIC tracks which interrupt is being handled in the ISR (In-Service Register). Until you send EOI:
- PIC won't send another interrupt of **equal or lower** priority
- System will hang if you forget!

**Why send to both PICs for slave IRQs?**
Slave IRQs (8-15) go through BOTH PICs:
```
Device → Slave PIC → (IRQ 2) → Master PIC → CPU
```
Both need to know the interrupt is complete.

**Priority system:**
- IRQ 0 = Highest priority
- IRQ 7 = Lowest (master)
- IRQ 8 = Highest (slave chain)
- IRQ 15 = Lowest overall

Timer (IRQ 0) will always preempt keyboard (IRQ 1).

---

## Phase 3: IRQ Handlers (ISRs 32-47)

### Overview
Now that PICs are remapped, we set up handlers for hardware interrupts (keyboard, timer, etc.).

---

### Step 3.1: Write 16 IRQ Stubs

**What it does:**
Creates assembly stubs for IRQ 0-15 (interrupts 32-47), similar to exception stubs.

**How it works:**

```asm
%macro IRQ 2
global irq%1
irq%1:
    push dword 0        ; No error code for IRQs
    push dword %2       ; Push interrupt number (32-47)
    jmp irq_common_stub
%endmacro

IRQ  0, 32     ; Timer
IRQ  1, 33     ; Keyboard
IRQ  2, 34     ; Cascade (never happens)
; ... etc ...
IRQ 15, 47     ; Secondary ATA
```

**Why pass interrupt number 32-47?**
After remapping, IRQ 0 triggers interrupt 32, IRQ 1 triggers 33, etc. This matches our IDT setup.

**irq_common_stub:**
```asm
irq_common_stub:
    pusha              ; Save registers
    mov ax, ds
    push eax
    
    mov ax, 0x10       ; Kernel data segment
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    
    push esp           ; Push register struct pointer
    call irq_handler   ; Call C handler
    
    add esp, 4
    
    pop eax
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    
    popa
    add esp, 8
    iret
```

Same pattern as ISR stubs - save state, call C, restore state.

---

### Step 3.2: Write irq_handler() - Dispatcher

**What it does:**
Dispatches to registered device-specific handlers and sends EOI to PIC.

**How it works:**

```c
void irq_handler(registers_t* regs) {
    // Call registered handler if exists
    if (interrupt_handlers[regs->int_no] != 0) {
        interrupt_handlers[regs->int_no](regs);
    }
    
    // Send EOI to PIC
    pic_send_eoi(regs->int_no - 32);  // Convert INT 32-47 to IRQ 0-15
}
```

**Why separate from isr_handler()?**

Different behavior:
- **Exceptions**: Usually fatal, print and halt
- **IRQs**: Normal operation, handle and continue

**Critical: EOI must be sent!**

```c
// ❌ WRONG - will hang after first interrupt!
void irq_handler(registers_t* regs) {
    if (interrupt_handlers[regs->int_no] != 0) {
        interrupt_handlers[regs->int_no](regs);
    }
    // Forgot to send EOI!
}

// ✅ CORRECT
void irq_handler(registers_t* regs) {
    if (interrupt_handlers[regs->int_no] != 0) {
        interrupt_handlers[regs->int_no](regs);
    }
    pic_send_eoi(regs->int_no - 32);  // Always send EOI!
}
```

**Can handlers be NULL?**
Yes! If no handler is registered, we still send EOI. The interrupt is acknowledged but ignored.

---

### Step 3.3: Write register_interrupt_handler()

**What it does:**
Allows device drivers to register callback functions for specific interrupts.

**How it works:**

```c
static isr_handler_t interrupt_handlers[256];

void register_interrupt_handler(uint8_t n, isr_handler_t handler) {
    interrupt_handlers[n] = handler;
}
```

**Usage example:**
```c
// In keyboard driver
void keyboard_callback(registers_t* regs) {
    char c = inb(0x60);  // Read key from keyboard port
    kprint_char(c);
}

keyboard_init() {
    register_interrupt_handler(33, keyboard_callback);  // IRQ 1 = INT 33
}
```

**Why use function pointers?**

Decoupling! The interrupt system doesn't need to know about keyboards, timers, etc. Drivers register themselves:

```
Generic interrupt system (isr.c)
         ↓    ↑
    Registers | Calls
         ↓    ↑
Specific drivers (keyboard.c, timer.c)
```

**Multiple handlers?**
Some systems support chained handlers (linked list). Your simple version allows one handler per interrupt - sufficient for most cases since each IRQ line typically has one device.

---

## Phase 4: Keyboard Driver

### Overview
Implement a practical example: handling keyboard interrupts (IRQ 1).

---

### Understanding Keyboard Hardware

**The 8042 Keyboard Controller:**
- Port 0x60: Data port (read scan codes)
- Port 0x64: Status/Command port

**How keyboard input works:**
1. User presses key
2. Keyboard sends scan code to controller
3. Controller triggers IRQ 1
4. CPU jumps to our handler (INT 33)
5. Handler reads port 0x60
6. Translates scan code to ASCII
7. Displays character

**Scan codes vs. ASCII:**
- Scan code: Hardware-specific key identifier (e.g., 0x1E = 'A' key)
- ASCII: Character representation (e.g., 0x41 = 'A' character)

Scan codes are layout-independent. QWERTY 'Q' and AZERTY 'A' are the same physical key = same scan code.

---

### Step 4.1: Define Scancode-to-ASCII Table

**What it does:**
Creates a lookup table to translate keyboard scan codes to ASCII characters.

**How it works:**

```c
static const char scancode_to_ascii[128] = {
    0,   27,  '1', '2', '3', '4', '5', '6',     // 0x00-0x07
    '7', '8', '9', '0', '-', '=', '\b', '\t',   // 0x08-0x0F
    'q', 'w', 'e', 'r', 't', 'y', 'u', 'i',     // 0x10-0x17
    'o', 'p', '[', ']', '\n', 0,   'a', 's',    // 0x18-0x1F
    // ... etc ...
};
```

**Why 128 entries?**
Scan codes are 7 bits (0-127):
- Bit 7 clear (0x00-0x7F): Key pressed ("make code")
- Bit 7 set (0x80-0xFF): Key released ("break code")

Example: Press 'A' = 0x1E, Release 'A' = 0x9E (0x1E | 0x80)

**Shift, Ctrl, Alt handling:**

Simple version (your Phase 4):
```c
char c = scancode_to_ascii[scancode];  // No modifier support
```

Advanced version:
```c
static bool shift_pressed = false;
static bool ctrl_pressed = false;

if (scancode == 0x2A || scancode == 0x36) {  // Shift pressed
    shift_pressed = true;
} else if (scancode == 0xAA || scancode == 0xB6) {  // Shift released
    shift_pressed = false;
} else {
    char c = shift_pressed ? 
             scancode_to_ascii_shift[scancode] : 
             scancode_to_ascii[scancode];
}
```

You'd need two tables for shift/non-shift versions.

---

### Step 4.2: Write keyboard_handler()

**What it does:**
Reads scan code from keyboard controller, translates to ASCII, displays character.

**How it works:**

```c
void keyboard_handler(registers_t* regs) {
    uint8_t scancode = inb(0x60);  // Read scan code from keyboard
    
    if (scancode & 0x80) {
        // Key release event - ignore for now
        return;
    }
    
    char c = scancode_to_ascii[scancode];
    if (c != 0) {
        kprint_char(c);  // Display character
    }
}
```

**Why read port 0x60?**
The keyboard controller stores the last scan code here. Reading it:
- Gets the scan code
- Clears the controller's buffer
- Allows next key press to be detected

**Why ignore releases?**
For basic input, we only care about key presses. Games and advanced UIs need releases too (e.g., to detect key held vs. tapped).

**Race condition consideration:**

```c
// ❌ POTENTIAL BUG
void keyboard_handler(registers_t* regs) {
    uint8_t scancode = inb(0x60);
    // If another key pressed here, it's lost!
    // ... slow processing ...
}

// ✅ BETTER - read immediately, process later
void keyboard_handler(registers_t* regs) {
    uint8_t scancode = inb(0x60);  // Clear hardware buffer ASAP
    keyboard_buffer[write_pos++] = scancode;  // Store in software buffer
}
```

For a simple OS, the fast version is fine.

---

### Step 4.3: Write keyboard_init()

**What it does:**
Registers the keyboard handler for IRQ 1.

**How it works:**

```c
void keyboard_init(void) {
    register_interrupt_handler(33, keyboard_handler);  // IRQ 1 = INT 33
}
```

**Why interrupt 33?**
After PIC remapping:
- IRQ 0 → INT 32
- IRQ 1 → INT 33  ← Keyboard!
- IRQ 2 → INT 34
- ...

**Could we use a different IRQ?**
No! The keyboard hardware is physically connected to IRQ 1. This is hardwired since the original IBM PC.

Modern systems use USB keyboards which have more flexibility, but in x86 legacy mode (what QEMU/BIOS provides), keyboard = IRQ 1.

---

## Phase 5: Integration

### Overview
Bring all pieces together in the kernel's main function.

---

### Step 5.1: Call Initialization Functions

**What it does:**
Calls init functions in the correct order during kernel startup.

**How it works:**

```c
void kernel_main(void) {
    clear_screen();
    kprint("Kernel started!\n");
    
    idt_init();         // 1. Set up IDT and exception handlers
    pic_remap(32, 40);  // 2. Remap PIC to avoid conflicts
    keyboard_init();    // 3. Register keyboard handler
    
    asm volatile("sti");  // 4. Enable interrupts
    
    kprint("Interrupts enabled. Press keys!\n");
    
    while(1) {
        // Idle loop - wait for interrupts
    }
}
```

**Why this specific order?**

```
1. idt_init()
   ↓ IDT is ready but interrupts still disabled (CLI during boot)
   ↓ CPU exceptions can be handled if they occur

2. pic_remap(32, 40)
   ↓ PICs now send IRQs to correct interrupt numbers
   ↓ No conflict with CPU exceptions

3. keyboard_init()
   ↓ Handler registered in interrupt_handlers[33]
   ↓ Still safe because interrupts disabled

4. sti
   ↓ Interrupts enabled!
   ↓ Keyboard works, exceptions handled
```

**What if we enabled interrupts earlier?**

```c
// ❌ DISASTER!
void kernel_main(void) {
    asm volatile("sti");     // Enable interrupts
    idt_init();              // OOPS! No IDT yet!
    // Timer IRQ 0 → INT 32 → IDT not loaded → Triple fault → Reboot!
}
```

---

### Step 5.2: Enable Interrupts with sti

**What it does:**
Clears the interrupt flag in EFLAGS, allowing the CPU to respond to hardware interrupts.

**How it works:**

```c
asm volatile("sti");  // SeT Interrupt flag
```

This single instruction changes one bit in the EFLAGS register:

```
EFLAGS before: IF (Interrupt Flag) = 0  →  Interrupts masked
EFLAGS after:  IF = 1                    →  Interrupts enabled
```

**What happens after sti?**

1. PIC checks for pending IRQs
2. If any IRQ active and not masked:
   - PIC signals CPU via INTR pin
   - CPU finishes current instruction
   - CPU checks EFLAGS.IF (now 1, so proceed)
   - CPU reads interrupt number from PIC
   - CPU looks up IDT entry
   - CPU saves state and jumps to handler

**cli vs. sti:**

```c
asm volatile("cli");  // CLear Interrupt flag - disable interrupts
// Critical section - interrupts can't disturb us
asm volatile("sti");  // Enable interrupts again
```

Use CLI for critical operations:
```c
void critical_operation(void) {
    asm volatile("cli");
    // Modify sensitive data structure
    // No interrupt can preempt us!
    asm volatile("sti");
}
```

**Are interrupts always enabled after sti?**

No! Interrupt gates (type 0xE) automatically disable interrupts when entered:
```
1. IRQ triggers → IF = 0 automatically
2. Handler executes with interrupts disabled
3. iret instruction restores old EFLAGS (IF = 1)
4. Interrupts enabled again
```

This prevents nested interrupts (interrupt during interrupt handler).

---

### Step 5.3: Update Makefile

**What it does:**
Adds new source files to the build system.

**How it works:**

```makefile
# Object files
OBJS = boot/boot_sector.o \
       boot/kernel_entry.o \
       kernel/idt.o \
       kernel/idt_load.o \      # New!
       kernel/isr_stubs.o \     # New!
       kernel/isr.o \           # New!
       kernel/pic.o \           # New!
       drivers/keyboard.o \     # New!
       drivers/screen.o \
       drivers/ports.o \
       kernel/kernel_main.o

# Compile C files
kernel/%.o: kernel/%.c
	gcc -ffreestanding -m32 -c $< -o $@

# Compile ASM files
kernel/%.o: kernel/%.asm
	nasm -f elf32 $< -o $@
```

**Why separate C and ASM rules?**
- C files: Compiled with GCC
- ASM files: Assembled with NASM
- Different tools, different rules

**What does each flag mean?**

`-ffreestanding`: Tell GCC we're not on a hosted environment (no libc)
`-m32`: Generate 32-bit x86 code
`-c`: Compile only, don't link yet
`-f elf32`: Output ELF 32-bit object format

**Linking:**
```makefile
kernel.bin: $(OBJS)
	ld -m elf_i386 -Ttext 0x1000 --oformat binary -o $@ $^
```

Links all .o files into final binary, placed at address 0x1000 in memory.

---

## The Complete Flow: From Keypress to Screen

Let's trace a single keyboard interrupt through your entire system:

### Event Timeline

**T=0: User Presses 'A' Key**
```
Physical keyboard → Scan code 0x1E sent to 8042 controller
```

**T=1: Keyboard Controller Signals PIC**
```
8042 controller → IRQ 1 signal to slave PIC
```

**T=2: PIC Signals CPU**
```
Slave PIC (IRQ 1) → Master PIC (IRQ 2) → CPU INTR pin
```

**T=3: CPU Begins Interrupt Sequence**
```
CPU:
  1. Finish current instruction
  2. Check EFLAGS.IF (enabled? yes)
  3. Acknowledge interrupt to PIC
  4. PIC sends interrupt number: 33 (remapped IRQ 1)
```

**T=4: CPU Consults IDT**
```
CPU:
  1. Calculate IDT entry: idtp.base + (33 * 8) = &idt[33]
  2. Read handler address from IDT entry
  3. Check privilege: entry DPL (0) vs current CPL (0) - OK
  4. Check present bit: 1 - OK
```

**T=5: CPU Saves State and Transfers Control**
```
CPU automatically pushes to stack:
  [SS]           (if privilege change)
  [ESP]          (if privilege change)
  [EFLAGS]       (contains IF=1)
  [CS]           (kernel code segment)
  [EIP]          (return address in kernel idle loop)

CPU then:
  - Clears EFLAGS.IF (disable interrupts)
  - Loads CS from IDT entry (0x08 = kernel code)
  - Jumps to handler address → irq1 stub
```

**T=6: irq1 Assembly Stub Executes**
```asm
irq1:
    push dword 0        ; Stack: [0]
    push dword 33       ; Stack: [33][0]
    jmp irq_common_stub
```

**T=7: irq_common_stub Saves Register State**
```asm
irq_common_stub:
    pusha               ; Stack: [regs][33][0][EIP][CS][EFLAGS]...
    mov ax, ds
    push eax            ; Stack: [DS][regs][33][0][EIP][CS][EFLAGS]...
    
    mov ax, 0x10        ; Load kernel data segment
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    
    push esp            ; Stack ptr → [registers_t* parameter]
    call irq_handler    ; Call C function
```

**T=8: irq_handler C Function Executes**
```c
void irq_handler(registers_t* regs) {
    // regs->int_no = 33
    // interrupt_handlers[33] = keyboard_handler
    
    if (interrupt_handlers[33] != 0) {
        keyboard_handler(regs);  ← Call keyboard driver!
    }
    
    pic_send_eoi(33 - 32);  // Send EOI for IRQ 1
}
```

**T=9: keyboard_handler Reads and Processes**
```c
void keyboard_handler(registers_t* regs) {
    uint8_t scancode = inb(0x60);  // Read 0x1E from port 0x60
    
    if (!(scancode & 0x80)) {  // Key press, not release
        char c = scancode_to_ascii[0x1E];  // = 'a'
        kprint_char('a');  // Display on screen!
    }
}
```

**T=10: pic_send_eoi Signals End**
```c
void pic_send_eoi(uint8_t irq) {
    // IRQ 1 < 8, so only master
    outb(PIC1_COMMAND, 0x20);  // "I'm done, send next interrupt"
}
```

**T=11: Return to irq_handler**
```c
    pic_send_eoi(33 - 32);  // Done
}    ← Return to assembly
```

**T=12: irq_common_stub Restores State**
```asm
    add esp, 4          ; Remove parameter
    
    pop eax             ; Restore old DS
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    
    popa                ; Restore all registers
    add esp, 8          ; Remove error code and IRQ number
    iret                ; Return from interrupt
```

**T=13: CPU Executes iret**
```
CPU pops from stack:
  [EIP]          → Resume idle loop
  [CS]           → Kernel code segment
  [EFLAGS]       → Restores IF=1 (interrupts enabled)
  
CPU continues at saved EIP in idle loop, as if nothing happened!
```

**T=14: User Sees 'a' on Screen**
```
Screen shows: "Interrupts enabled. Press keys!a"
                                                ↑ new character!
```

---

## Advanced Topics & Optimizations

### Critical Sections

**Problem:** What if an interrupt modifies data you're reading?

```c
// ❌ RACE CONDITION
char buffer[256];
int write_pos = 0;
int read_pos = 0;

void add_to_buffer(char c) {
    buffer[write_pos++] = c;  // Interrupt could happen here!
}

char read_from_buffer() {
    return buffer[read_pos++];  // Or here!
}
```

**Solution:** Disable interrupts during critical operations:

```c
// ✅ SAFE
void add_to_buffer(char c) {
    cli();
    buffer[write_pos++] = c;
    sti();
}
```

Or use atomic operations / locks in more advanced kernels.

---

### Interrupt Priorities

Hardware priority (from highest to lowest):
1. IRQ 0 - System Timer (must be precise!)
2. IRQ 1 - Keyboard
3. IRQ 8 - Real-Time Clock
4. IRQ 9-15 - Various peripherals
5. IRQ 3 - COM2
6. IRQ 4 - COM1
7. IRQ 5 - Sound/LPT2
8. IRQ 6 - Floppy
9. IRQ 7 - LPT1

If timer and keyboard interrupt simultaneously, timer wins.

---

### Interrupt Masking

**Disable specific IRQ:**
```c
void irq_disable(uint8_t irq) {
    uint16_t port = (irq < 8) ? PIC1_DATA : PIC2_DATA;
    uint8_t value = inb(port);
    value |= (1 << (irq % 8));  // Set mask bit
    outb(port, value);
}

void irq_enable(uint8_t irq) {
    uint16_t port = (irq < 8) ? PIC1_DATA : PIC2_DATA;
    uint8_t value = inb(port);
    value &= ~(1 << (irq % 8));  // Clear mask bit
    outb(port, value);
}
```

**Why mask?**
- Debugging: Isolate specific device
- Power saving: Disable unused devices
- Critical sections: Block specific interrupt source

**Masked vs. Disabled:**
- CLI: Disables ALL interrupts
- Masking: Disables specific IRQ line

---

### Modern Replacements

**APIC (Advanced PIC):**
- Replaces 8259 PIC on modern systems
- Supports more interrupts (224+)
- Better multicore support
- Message Signaled Interrupts (MSI)

**MSI (Message Signaled Interrupts):**
- PCIe devices write to memory-mapped address
- No IRQ lines needed!
- Better performance, no sharing

Most modern OSes use APIC, but learning PIC teaches fundamental concepts.

---

## Common Debugging Scenarios

### Triple Fault (Immediate Reboot)

**Symptoms:** QEMU/system resets immediately

**Causes:**
1. IDT not loaded before sti
2. Exception during exception handler
3. Invalid stack pointer
4. Handler not properly defined

**Debug:**
```c
// Add to exception handler
kprint("Exception ");
kprint_hex(regs->int_no);
for (;;) asm("hlt");  // Don't return!
```

---

### No Keyboard Response

**Checklist:**
- [ ] PIC remapped? (pic_remap called)
- [ ] Handler registered? (register_interrupt_handler(33, ...))
- [ ] Interrupts enabled? (sti executed)
- [ ] IRQ 1 not masked? (check PIC1_DATA bit 1)
- [ ] Reading port 0x60? (clears keyboard buffer)

**Test with timer instead:**
```c
void timer_handler(registers_t* regs) {
    static int tick = 0;
    kprint_hex(tick++);  // Should count up
}

register_interrupt_handler(32, timer_handler);  // IRQ 0
```

Timer triggers automatically ~18 times/second.

---

### Infinite IRQs

**Symptoms:** Handler called repeatedly without user action

**Cause:** Forgot to send EOI

**Fix:**
```c
void irq_handler(registers_t* regs) {
    // ... handle interrupt ...
    pic_send_eoi(regs->int_no - 32);  ← DON'T FORGET!
}
```

---

## Summary

### What You've Learned

1. **Interrupts** are hardware/CPU mechanisms to pause execution and handle events
2. **IDT** maps interrupt numbers to handler functions
3. **PIC** manages hardware interrupt requests from devices
4. **Remapping** prevents IRQ/exception conflicts
5. **Assembly stubs** create uniform stack layout for C handlers
6. **EOI** tells PIC interrupt is complete
7. **Integration** requires careful initialization order

### The Power of Interrupts

Without interrupts:
```c
while (1) {
    poll_keyboard();   // Waste CPU
    poll_network();    // Waste CPU
    poll_disk();       // Waste CPU
    // 99% wasted checking!
}
```

With interrupts:
```c
while (1) {
    hlt;  // Sleep until interrupt
    // Keyboard: handled!
    // Network: handled!
    // Disk: handled!
    // 0% wasted, instant response!
}
```

Interrupts enable:
- Multitasking (timer preemption)
- Device I/O (keyboard, disk, network)
- Error handling (page faults, exceptions)
- Power management (sleep until event)

**You've built the foundation for a real operating system!** 🎉

---

## Further Reading

- Intel 64 and IA-32 Architectures Software Developer Manual, Volume 3A (Chapter 6: Interrupt and Exception Handling)
- OSDev Wiki: https://wiki.osdev.org/Interrupts
- "Protected Mode" tutorial by Brendan: https://wiki.osdev.org/Protected_Mode
- Linux kernel source: `arch/x86/kernel/irq.c` (see how Linux does it!)

---

*This guide was created for educational purposes as part of the ApPa OS project.*

