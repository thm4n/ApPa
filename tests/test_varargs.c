#include "../klibc/stdarg.h"
#include "../drivers/screen.h"

// Simple test function to demonstrate va_list
void test_varargs(const char* first, ...) {
    va_list args;
    va_start(args, first);
    
    kprint("First: ");
    kprint((char*)first);
    kprint("\n");
    
    // Get second argument (int)
    int second = va_arg(args, int);
    kprint("Second (int): ");
    kprint_uint(second);
    kprint("\n");
    
    // Get third argument (string)
    char* third = va_arg(args, char*);
    kprint("Third (string): ");
    kprint(third);
    kprint("\n");
    
    va_end(args);
}

void test_va_system() {
    kprint("\n=== Testing va_list system ===\n");
    test_varargs("Hello", 42, "World");
}
