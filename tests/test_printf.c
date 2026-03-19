#include "../libc/stdio.h"
#include "../libc/stddef.h"
#include "../drivers/screen.h"
#include "test_printf.h"

void test_printf() {
    kprint("\n=== Testing kprintf ===\n");
    kprint("About to test signed integers...\n");
    
    // Test 1: Signed integers
    kprint("Test 1: Signed integers (%d)\n");
    kprint("Testing positive...\n");
    kprintf("  Positive: %d\n", 42);
    kprint("Done with positive\n");
    
    kprintf("  Negative: %d\n", -42);
    kprintf("  Zero: %d\n", 0);
    kprintf("  Large: %d\n", 2147483647);
    kprintf("  Large negative: %d\n", -2147483648);
    kprint("  [PASS] Signed integers\n");
    
    // Test 2: Unsigned integers
    kprint("Test 2: Unsigned integers (%u)\n");
    kprintf("  Small: %u\n", 42);
    kprintf("  Zero: %u\n", 0);
    kprintf("  Max: %u\n", 4294967295);
    kprint("  [PASS] Unsigned integers\n");
    
    // Test 3: Hexadecimal (lowercase)
    kprint("Test 3: Hexadecimal lowercase (%x)\n");
    kprintf("  0xDEADBEEF: %x\n", 0xDEADBEEF);
    kprintf("  0x0: %x\n", 0x0);
    kprintf("  0xFF: %x\n", 0xFF);
    kprintf("  0x12345678: %x\n", 0x12345678);
    kprint("  [PASS] Hex lowercase\n");
    
    // Test 4: Hexadecimal (uppercase)
    kprint("Test 4: Hexadecimal uppercase (%X)\n");
    kprintf("  0xDEADBEEF: %X\n", 0xDEADBEEF);
    kprintf("  0xCAFEBABE: %X\n", 0xCAFEBABE);
    kprint("  [PASS] Hex uppercase\n");
    
    // Test 5: Characters
    kprint("Test 5: Characters (%c)\n");
    kprintf("  A: %c\n", 'A');
    kprintf("  Z: %c\n", 'Z');
    kprintf("  0: %c\n", '0');
    kprintf("  Space: '%c'\n", ' ');
    kprint("  [PASS] Characters\n");
    
    // Test 6: Strings
    kprint("Test 6: Strings (%s)\n");
    kprintf("  Hello: %s\n", "Hello");
    kprintf("  World: %s\n", "World!");
    kprintf("  Empty: '%s'\n", "");
    kprintf("  NULL: %s\n", NULL);
    kprint("  [PASS] Strings\n");
    
    // Test 7: Pointers
    kprint("Test 7: Pointers (%p)\n");
    int x = 42;
    kprintf("  Address of x: %p\n", &x);
    kprintf("  NULL pointer: %p\n", NULL);
    kprintf("  High address: %p\n", (void*)0xDEADBEEF);
    kprint("  [PASS] Pointers\n");
    
    // Test 8: Percent sign
    kprint("Test 8: Literal percent (%%)\n");
    kprintf("  100%% complete\n");
    kprintf("  50%% done, 50%% to go\n");
    kprint("  [PASS] Percent sign\n");
    
    // Test 9: Mixed format strings
    kprint("Test 9: Mixed format specifiers\n");
    kprintf("  Int %d, Hex 0x%x, Str '%s', Char '%c'\n", 
            42, 0xFF, "test", 'X');
    kprintf("  Name: %s, Age: %d, ID: 0x%X\n", 
            "Alice", 25, 0xABCD);
    kprint("  [PASS] Mixed formats\n");
    
    // Test 10: Edge cases
    kprint("Test 10: Edge cases\n");
    kprintf("  Just text, no specifiers\n");
    kprintf("  Multiple %% %% percent signs\n");
    kprintf("  Unknown specifier: %z should show '%%z'\n");
    kprint("  [PASS] Edge cases\n");
    
    kprint("All kprintf tests completed!\n");
}
