# kprintf() Quick Reference

## Function Signature
```c
void kprintf(const char* format, ...);
```

## Supported Format Specifiers

| Specifier | Type | Description | Example |
|-----------|------|-------------|---------|
| `%d`, `%i` | `int` | Signed decimal integer | `kprintf("%d", -42)` → `-42` |
| `%u` | `unsigned int` | Unsigned decimal integer | `kprintf("%u", 4294967295)` → `4294967295` |
| `%x` | `unsigned int` | Lowercase hexadecimal | `kprintf("%x", 0xDEAD)` → `dead` |
| `%X` | `unsigned int` | Uppercase hexadecimal | `kprintf("%X", 0xBEEF)` → `BEEF` |
| `%c` | `char` | Single character | `kprintf("%c", 'A')` → `A` |
| `%s` | `char*` | Null-terminated string | `kprintf("%s", "Hi")` → `Hi` |
| `%p` | `void*` | Pointer address | `kprintf("%p", ptr)` → `0x1000` |
| `%%` | - | Literal percent sign | `kprintf("100%%")` → `100%` |

## Usage Examples

### Basic Printing
```c
kprintf("Hello, World!\n");
kprintf("The answer is %d\n", 42);
kprintf("Name: %s, Age: %d\n", "Alice", 25);
```

### Numbers in Different Formats
```c
int value = 255;
kprintf("Decimal: %d\n", value);      // 255
kprintf("Hex (lower): %x\n", value);  // ff
kprintf("Hex (upper): %X\n", value);  // FF
```

### Debugging with Multiple Values
```c
kprintf("x=%d, y=%d, sum=%d\n", x, y, x + y);
kprintf("Status: %s, Code: 0x%X\n", "OK", status_code);
```

### Pointer Addresses
```c
int* ptr = &variable;
kprintf("Address: %p, Value: %d\n", ptr, *ptr);
```

### NULL Handling
```c
char* str = NULL;
kprintf("String: %s\n", str);  // Prints: String: (null)
```

## Include Files Required
```c
#include "../libc/stdio.h"  // For kprintf()
```

## Common Patterns

### Error Messages
```c
kprintf("[ERROR] Failed to allocate %u bytes\n", size);
```

### Debug Output
```c
kprintf("[DEBUG] Function called with args: %d, %s, 0x%X\n", arg1, arg2, arg3);
```

### Status Reporting
```c
kprintf("[OK] %s initialized successfully\n", module_name);
```

## Implementation Details

### Files
- **Header:** `libc/stdio.h`
- **Implementation:** `libc/stdio.c`
- **Dependencies:** `libc/stdarg.h`, `libc/stddef.h`, `drivers/screen.h`

### Number Conversion
- Uses `itoa()` for signed integers
- Uses `utoa()` for unsigned integers and hex
- Supports bases 2-36

### String Handling
- NULL strings print as "(null)"
- Empty strings print as empty (no output)

## Technical Notes

### Type Promotion
Small types are promoted when passed as varargs:
```c
// CORRECT:
char c = 'A';
kprintf("%c", c);  // c is promoted to int automatically

// In kprintf implementation:
char c = (char)va_arg(args, int);  // Read as int, cast to char
```

### Buffer Size
- Internal conversion buffer: 32 bytes
- Sufficient for 32-bit integers in any base
- No buffer overflow risk for standard types

### Performance
- Character-by-character output via `kprint()`
- No internal buffering
- Immediate display on VGA screen

## Testing

Comprehensive tests in `tests/test_printf.c`:
- All format specifiers
- Edge cases (NULL, zero, negative, max values)
- Mixed format strings
- Literal percent signs

Run tests:
```bash
make run  # Tests run automatically at boot
```

## Future Enhancements

Potential additions (not yet implemented):
- Width specifiers: `%5d` (right-align to width 5)
- Precision: `%.2d` (zero-pad to 2 digits)
- Left-align: `%-5d`
- Octal: `%o`
- Binary: `%b`
- Floating point: `%f` (requires float support)
- sprintf(): Format to string buffer
- snprintf(): Safe version with size limit

## Related Functions

### String Utilities (libc/string.h)
```c
uint32_t strlen(const char* str);
int strcmp(const char* s1, const char* s2);
char* strcpy(char* dest, const char* src);
char* strncpy(char* dest, const char* src, uint32_t n);
void* memset(void* ptr, int value, uint32_t num);
void* memcpy(void* dest, const void* src, uint32_t n);
```

### Number Conversion (libc/stdio.h)
```c
void itoa(int32_t value, char* str, int base);
void utoa(uint32_t value, char* str, int base);
```

## See Also
- [Printf Implementation Plan.md](Printf%20Implementation%20Plan.md) - Full implementation guide
- [va_list Technical Reference.md](va_list%20Technical%20Reference.md) - Variable arguments details
- [tests/test_printf.c](../tests/test_printf.c) - Test suite
