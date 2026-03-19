# Phase 7: Simple Command Shell

**Status:** ✅ COMPLETED  
**Priority:** High  
**Estimated Time:** 1 day  
**Actual Time:** 1 day

---

## Overview

Phase 7 introduces an interactive command-line shell to the ApPa kernel, transforming basic keyboard input into a fully functional command interface. The shell provides a structured way for users to interact with the kernel through text-based commands.

### Why a Shell?

The command shell serves several critical purposes:
1. **Practical User Interface** - Makes keyboard input actually useful for system interaction
2. **Command Parsing Practice** - Demonstrates string manipulation and parsing techniques
3. **Extensibility** - Provides a framework for adding new commands easily
4. **Testing Interface** - Allows interactive testing of kernel features
5. **Foundation for Future Features** - Sets up infrastructure for scripting and automation

### Design Philosophy

The shell is designed to be:
- **Simple** - Basic command parsing without complex syntax
- **Memory-Efficient** - Uses fixed-size buffer (256 bytes) to avoid fragmentation
- **Modular** - Each command is a separate function for easy extension
- **Interactive** - Immediate feedback and error messages
- **Colorful** - Supports VGA text colors for visual customization

---

## Architecture

### Components

```
┌─────────────────────────────────────┐
│        Keyboard Driver              │
│   (drivers/keyboard.c)              │
│   - Reads scan codes                │
│   - Translates to ASCII             │
└──────────┬──────────────────────────┘
           │ ASCII character
           ▼
┌─────────────────────────────────────┐
│         Shell System                │
│      (kernel/shell.c)               │
│   ┌─────────────────────────────┐   │
│   │  shell_input()              │   │
│   │  - Buffers characters       │   │
│   │  - Handles backspace/enter  │   │
│   └──────┬──────────────────────┘   │
│          │ Command line (on Enter)  │
│          ▼                           │
│   ┌─────────────────────────────┐   │
│   │  shell_execute()            │   │
│   │  - Parses command           │   │
│   │  - Dispatches to handler    │   │
│   └──────┬──────────────────────┘   │
│          │                           │
│          ▼                           │
│   ┌─────────────────────────────┐   │
│   │  Command Handlers           │   │
│   │  - cmd_help()               │   │
│   │  - cmd_clear()              │   │
│   │  - cmd_echo()               │   │
│   │  - cmd_mem()                │   │
│   │  - cmd_color()              │   │
│   └─────────────────────────────┘   │
└─────────────────────────────────────┘
```

### Data Flow

1. **Keyboard Input** → Scan code from PS/2 keyboard (IRQ1)
2. **ASCII Translation** → Keyboard driver converts to ASCII
3. **Shell Buffering** → `shell_input()` accumulates characters
4. **Command Parsing** → On Enter, `shell_execute()` parses buffer
5. **Command Execution** → Dispatcher calls appropriate handler
6. **Output** → Command results displayed via screen driver

---

## Implementation Details

### File Structure

```
kernel/
├── shell.h          # Shell API and constants
└── shell.c          # Shell implementation and command handlers

drivers/
├── screen.c         # Enhanced with color support
└── screen.h         # Added VGA color constants

libc/
├── string.c         # Added strncmp() for command parsing
└── string.h         # Added strncmp() declaration
```

### Key Constants

```c
#define SHELL_BUFFER_SIZE 256    // Command buffer size (256 bytes)
```

### Core Functions

#### `shell_init()`
Initializes the shell system by resetting the command buffer.

**Called:** During kernel startup, after keyboard initialization

#### `shell_input(char c)`
Processes individual characters from keyboard input.

**Handles:**
- **Backspace (`\b`)** - Removes character from buffer and screen
- **Enter (`\n`)** - Executes command and resets buffer
- **Normal characters** - Adds to buffer and echoes to screen

**Buffer Management:**
- Prevents overflow (max 255 characters + null terminator)
- Null-terminates for safe string operations

#### `shell_execute(const char* cmd)`
Parses and dispatches commands.

**Parsing Strategy:**
1. Skip leading whitespace
2. Find first space (separates command from arguments)
3. Extract command name and arguments
4. Match against known commands using `strncmp()`
5. Call appropriate handler or display error

**Example:**
```
Input: "color green black"
       ├────┘ └──────────┘
       cmd    args
```

---

## Available Commands

### `help`
**Syntax:** `help`  
**Description:** Display list of available commands with brief usage information.

**Implementation:** Simple text output, no arguments.

**Example:**
```
> help
Available commands:
  help         - Show this help message
  clear        - Clear the screen
  echo <text>  - Print text to screen
  mem          - Display memory allocation statistics
  color <name> - Change text color
               Colors: white, red, green, blue, yellow,
                       cyan, magenta, grey, black
```

---

### `clear`
**Syntax:** `clear`  
**Description:** Clear the screen and reset cursor to top-left position.

**Implementation:** Calls `clear_screen()` from screen driver.

**Use Cases:**
- Remove clutter from terminal
- Reset display before running new commands
- Clean slate for demonstrations

**Example:**
```
> clear
[screen cleared]
```

---

### `echo <text>`
**Syntax:** `echo <text>`  
**Description:** Print the specified text to the screen.

**Arguments:**
- `<text>` - Any text following the "echo" command

**Implementation:** Prints argument string followed by newline.

**Use Cases:**
- Display messages
- Test text output
- Simple status indicators
- Future: Script output

**Examples:**
```
> echo Hello, ApPa!
Hello, ApPa!

> echo System initialized successfully
System initialized successfully
```

---

### `mem`
**Syntax:** `mem`  
**Description:** Display memory allocation statistics from the kernel heap.

**Implementation:** Calls `kmalloc_status()` from memory allocator.

**Output Information:**
- Total heap size
- Number of free blocks
- Number of allocated blocks
- Largest free block size
- Total free memory
- Total allocated memory
- Fragmentation statistics

**Use Cases:**
- Debug memory leaks
- Monitor heap usage
- Verify allocation/deallocation
- Performance analysis

**Example:**
```
> mem
=== Kernel Heap Status ===
Heap region: 0x00100000 - 0x00200000 (1048576 bytes)
Free blocks: 1
Allocated blocks: 0
Largest free block: 1048560 bytes
Total free: 1048560 bytes
Total allocated: 0 bytes
```

---

### `color <foreground> [background]`
**Syntax:**  
- `color <foreground>`  
- `color <foreground> <background>`

**Description:** Change the text color for subsequent output.

**Arguments:**
- `<foreground>` - Foreground (text) color name (required)
- `<background>` - Background color name (optional, defaults to black)

**Available Colors:**
- `black` - #000000
- `blue` - Dark blue
- `green` - Dark green
- `cyan` - Dark cyan
- `red` - Dark red
- `magenta` - Dark magenta
- `brown` - Brown/dark yellow
- `grey` - Light grey (default text color)
- `darkgrey` - Dark grey
- `lightblue` - Bright blue
- `lightgreen` - Bright green
- `lightcyan` - Bright cyan
- `lightred` - Bright/pink red
- `lightmagenta` - Bright magenta/pink
- `yellow` - Bright yellow
- `white` - Bright white

**VGA Color System:**
VGA text mode uses 4-bit foreground and 4-bit background colors:
- Color byte format: `(background << 4) | foreground`
- Foreground: 16 colors (4 bits)
- Background: 8 colors (3 bits, blink bit typically ignored)

**Implementation Details:**
1. Parse color name(s) from arguments
2. Look up color code using `parse_color_name()`
3. Combine into VGA color byte using `VGA_COLOR(fg, bg)` macro
4. Update global `current_color` via `set_text_color()`
5. All subsequent text uses new color

**Use Cases:**
- Highlight warnings (red text)
- Emphasize success messages (green text)
- Create visual themes
- Color-code different output types
- Improve readability

**Examples:**
```
> color green
Color changed.
[text now appears in green]

> color yellow black
Color changed.
[text now appears in yellow on black background]

> color white blue
Color changed.
[text now appears in white on blue background]

> color invalid
Invalid foreground color: invalid

> color red purple
Invalid background color: purple
```

**Technical Implementation:**

```c
// Color parsing
static uint8_t parse_color_name(const char* name) {
    if (strcmp(name, "black") == 0) return COLOR_BLACK;
    if (strcmp(name, "green") == 0) return COLOR_GREEN;
    // ... other colors
    return 0xFF; // Invalid
}

// Command handler
static void cmd_color(const char* args) {
    // Parse foreground and optional background
    uint8_t fg = parse_color_name(fg_name);
    uint8_t bg = parse_color_name(bg_name);  // or COLOR_BLACK default
    
    // Update current color
    set_text_color(VGA_COLOR(fg, bg));
}
```

**Color Persistence:**
- Color setting persists until changed again
- Survives `clear` command (screen cleared with current color)
- Applied to all output: shell prompt, command results, errors

---

## Color System Architecture

### Screen Driver Enhancements

**Added to `drivers/screen.h`:**

```c
// VGA Color Constants (0x0 - 0xF)
#define COLOR_BLACK         0x0
#define COLOR_BLUE          0x1
#define COLOR_GREEN         0x2
#define COLOR_CYAN          0x3
#define COLOR_RED           0x4
#define COLOR_MAGENTA       0x5
#define COLOR_BROWN         0x6
#define COLOR_LIGHT_GREY    0x7
#define COLOR_DARK_GREY     0x8
#define COLOR_LIGHT_BLUE    0x9
#define COLOR_LIGHT_GREEN   0xA
#define COLOR_LIGHT_CYAN    0xB
#define COLOR_LIGHT_RED     0xC
#define COLOR_LIGHT_MAGENTA 0xD
#define COLOR_YELLOW        0xE
#define COLOR_WHITE         0xF

// Helper macro: combine foreground and background
#define VGA_COLOR(fg, bg) ((bg << 4) | fg)

// Color management functions
void set_text_color(char color);
char get_text_color();
```

**Added to `drivers/screen.c`:**

```c
// Global current color (default: white on black)
static char current_color = WHITE_ON_BLACK;

void set_text_color(char color) {
    current_color = color;
}

char get_text_color() {
    return current_color;
}
```

**Modified Functions:**
All functions that write to VGA memory now use `current_color` instead of hardcoded `WHITE_ON_BLACK`:
- `clear_screen()` - Clears with current color
- `kprint_at()` - Prints with current color
- `kprint_backspace()` - Erases with current color
- `scroll_screen()` - Scrolls with current color
- `print_char()` - Uses current color as default

---

## String Utility Additions

### `strncmp()`
**Purpose:** Compare first N characters of two strings (needed for command parsing).

**Signature:**
```c
int strncmp(const char* s1, const char* s2, uint32_t n);
```

**Returns:**
- `< 0` if s1 < s2
- `0` if s1 == s2 (first n characters match)
- `> 0` if s1 > s2

**Use in Shell:**
```c
if (strncmp(cmd, "help", cmd_len) == 0 && cmd_len == 4) {
    cmd_help();
}
```

**Why Needed:**
- `strcmp()` compares entire strings
- Commands may have arguments: "echo hello" vs "echo"
- Need to extract and compare just the command name

---

## Integration Points

### Kernel Startup Sequence

Updated `kernel_main.c`:

```c
void main() {
    serial_init();
    clear_screen();
    kprint("ApPa Kernel v0.1\n");
    
    // Phase 1-3: IDT, PIC, Heap
    idt_init();
    pic_remap(32, 40);
    kmalloc_init();
    
    // Phase 4: Keyboard
    keyboard_init();
    
    // Phase 5: Shell ← NEW
    shell_init();
    kprint("  [OK] Shell initialized\n");
    
    // Phase 6: Enable interrupts
    __asm__ volatile("sti");
    kprint("  [OK] Interrupts enabled\n\n");
    
    run_all_tests();
    
    kprint("\nApPa Shell v0.1 - Type 'help' for commands\n");
    kprint("> ");
    
    while (1) {
        __asm__ volatile("hlt");
    }
}
```

### Keyboard Integration

Updated `drivers/keyboard.c`:

```c
#include "../kernel/shell.h"

static void keyboard_handler(registers_t* regs) {
    uint8_t scancode = port_byte_in(KEYBOARD_DATA_PORT);
    
    if (scancode & 0x80) return;  // Key release
    
    // Special keys (F1 for log dump)
    if (scancode == 0x3B) {
        kprint("\n");
        klog_dump();
        kprint("> ");
        return;
    }
    
    // Translate and pass to shell
    char ascii = scancode_to_ascii[scancode];
    if (ascii != 0) {
        shell_input(ascii);  // ← Shell handles all input
    }
}
```

---

## Testing

### Basic Command Tests

1. **Help Command:**
   ```
   > help
   [verify all commands listed]
   ```

2. **Clear Command:**
   ```
   > clear
   [screen should clear]
   ```

3. **Echo Command:**
   ```
   > echo Hello World
   Hello World
   ```

4. **Memory Statistics:**
   ```
   > mem
   [verify heap statistics display]
   ```

5. **Color Command:**
   ```
   > color green
   Color changed.
   > color yellow black
   Color changed.
   > color white
   Color changed.
   ```

### Edge Cases

1. **Empty Input:**
   ```
   > [press Enter]
   > [should just show new prompt]
   ```

2. **Unknown Command:**
   ```
   > invalid
   Unknown command: invalid
   Type 'help' for available commands.
   ```

3. **Command with Extra Spaces:**
   ```
   >    echo    test
   test
   ```

4. **Buffer Overflow:**
   - Type 256+ characters
   - Should stop accepting at 255 characters
   - Should not crash or corrupt memory

5. **Invalid Color:**
   ```
   > color invalid
   Invalid foreground color: invalid
   ```

6. **Backspace Handling:**
   - Type characters
   - Press backspace multiple times
   - Verify proper deletion from buffer and screen

---

## Memory Usage

### Static Memory

- **Command Buffer:** 256 bytes (fixed, in `.bss` section)
- **Color String Buffers:** ~64 bytes temporary stack (during color parsing)

### Heap Memory

- **None** - Shell uses no dynamic allocation
- All buffers are fixed-size to avoid fragmentation

### Stack Usage

- Minimal stack usage per function
- No recursive calls
- String operations use fixed-size temporary buffers

---

## Future Enhancements

### Planned Commands

1. **`uptime`**
   - Requires: Timer driver (Phase 8)
   - Display: System uptime in seconds/minutes

2. **`reboot`**
   - Requires: Keyboard controller magic
   - Action: Soft reboot system

3. **`date`** / **`time`**
   - Requires: RTC (Real-Time Clock) driver
   - Display: Current date/time

4. **`ps`** / **`tasks`**
   - Requires: Multitasking (Phase 10+)
   - Display: Running processes

5. **`kill <pid>`**
   - Requires: Multitasking
   - Action: Terminate process

### Feature Additions

1. **Command History**
   - Arrow up/down navigation
   - Requires: Keyboard scan code handling for arrow keys
   - Storage: Circular buffer of previous commands

2. **Tab Completion**
   - Tab key triggers command completion
   - Match partial commands against known set

3. **Piping / Redirection**
   - `command1 | command2`
   - Output redirection to serial port

4. **Scripting**
   - Load commands from file
   - Execute batch operations

5. **Aliases**
   - User-defined command shortcuts
   - `alias ll='mem'`

6. **Environment Variables**
   - `$HOME`, `$PATH`, etc.
   - Command substitution

---

## Common Issues and Solutions

### Issue: Commands not recognized
**Symptom:** Every command shows "Unknown command"  
**Cause:** `strncmp()` not linked or incorrect comparison  
**Solution:** Verify `strncmp()` in `libc/string.c` and ensure makefile includes all object files

### Issue: Colors don't change
**Symptom:** `color` command runs but text stays white  
**Cause:** `current_color` not updated or screen driver not using it  
**Solution:** Check `set_text_color()` updates global and `kprint_at()` uses `current_color`

### Issue: Backspace deletes prompt
**Symptom:** Backspace can erase the "> " prompt  
**Cause:** No boundary check in `kprint_backspace()`  
**Current:** Checks `offset > 0`, but should track prompt position  
**Solution:** Store prompt start offset and prevent backspace before it (future enhancement)

### Issue: Buffer overflow
**Symptom:** Kernel crashes when typing 256+ characters  
**Cause:** Missing bounds check in `shell_input()`  
**Solution:** Verify `buffer_pos < SHELL_BUFFER_SIZE - 1` check exists

### Issue: Color names case-sensitive
**Symptom:** "Green" doesn't work but "green" does  
**Current Behavior:** Case-sensitive comparison  
**Enhancement:** Convert to lowercase before `parse_color_name()`

---

## Performance Characteristics

### Time Complexity

- **Input Processing:** O(1) per character
- **Command Parsing:** O(n) where n = command length
- **Command Matching:** O(c) where c = number of commands
- **Color Parsing:** O(k) where k = number of color names

### Space Complexity

- **Command Buffer:** O(1) - fixed 256 bytes
- **No Dynamic Allocation:** No heap fragmentation from shell

### Best/Worst Case

**Best Case:**  
- Short commands (1-4 chars)
- First command in match list
- Time: ~microseconds

**Worst Case:**  
- 255-character command
- No match (unknown command)
- Time: still ~microseconds (still very fast)

---

## Code Quality

### Design Principles Applied

✅ **Separation of Concerns**
- Shell logic independent of keyboard hardware
- Command handlers modular and independent

✅ **DRY (Don't Repeat Yourself)**
- Color parsing extracted to separate function
- Command template consistent

✅ **Error Handling**
- Invalid commands: helpful error message
- Invalid colors: specific error with color name
- Buffer overflow: silent prevention

✅ **Readability**
- Clear function names (`cmd_help`, `cmd_color`)
- Comprehensive comments
- Logical organization

✅ **Testability**
- Each command is independent function
- Easy to unit test individually

---

## Educational Value

### Concepts Demonstrated

1. **String Parsing**
   - Tokenization (splitting on whitespace)
   - String comparison (`strcmp`, `strncmp`)
   - Argument extraction

2. **Command Pattern**
   - Dispatcher architecture
   - Function pointers (implicit via if-else chain)
   - Extensible command framework

3. **State Management**
   - Global state (command buffer, buffer position)
   - Color state (current_color)

4. **User Interface Design**
   - Prompts and feedback
   - Error messages
   - Help documentation

5. **Integration**
   - Multiple subsystems working together
   - Clean interfaces between layers

---

## Conclusion

Phase 7 successfully transforms the ApPa kernel from a simple keyboard echo system into an interactive operating system with a functional command-line interface. The shell provides:

- ✅ **Interactive Command Execution**
- ✅ **Extensible Architecture** - Easy to add new commands
- ✅ **User-Friendly Interface** - Help, error messages, color customization
- ✅ **Memory Efficient** - No dynamic allocation, fixed buffers
- ✅ **Robust** - Handles edge cases, prevents crashes

The shell establishes a foundation for future kernel features that require user interaction and serves as a practical testing interface for all kernel subsystems.

**Next Phase:** Timer Driver (PIT/IRQ0) for system uptime tracking and future multitasking scheduler.
