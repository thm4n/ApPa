#include "stdio.h"
#include "stddef.h"
#include "stdarg.h"
#include "../drivers/screen.h"

// Helper: Reverse a string in place
static void reverse(char* str, int length) {
    int start = 0;
    int end = length - 1;
    while (start < end) {
        char temp = str[start];
        str[start] = str[end];
        str[end] = temp;
        start++;
        end--;
    }
}

// Convert unsigned integer to string in given base (2-36)
void utoa(uint32_t value, char* str, int base) {
    int i = 0;
    
    // Handle 0 explicitly
    if (value == 0) {
        str[i++] = '0';
        str[i] = '\0';
        return;
    }
    
    // Process individual digits
    while (value != 0) {
        int rem = value % base;
        str[i++] = (rem > 9) ? (rem - 10) + 'a' : rem + '0';
        value = value / base;
    }
    
    str[i] = '\0';
    reverse(str, i);
}

// Convert signed integer to string in given base
void itoa(int32_t value, char* str, int base) {
    int i = 0;
    int is_negative = 0;
    
    // Handle 0 explicitly
    if (value == 0) {
        str[i++] = '0';
        str[i] = '\0';
        return;
    }
    
    // Handle negative numbers (only for base 10)
    if (value < 0 && base == 10) {
        is_negative = 1;
        value = -value;
    }
    
    // Process individual digits
    while (value != 0) {
        int rem = value % base;
        str[i++] = (rem > 9) ? (rem - 10) + 'a' : rem + '0';
        value = value / base;
    }
    
    // Add negative sign
    if (is_negative)
        str[i++] = '-';
    
    str[i] = '\0';
    reverse(str, i);
}

// Printf implementation
void kprintf(const char* format, ...) {
    va_list args;
    va_start(args, format);
    
    char buffer[32];  // Buffer for number conversions
    
    for (int i = 0; format[i] != '\0'; i++) {
        if (format[i] == '%' && format[i + 1] != '\0') {
            i++;  // Move to format specifier
            
            switch (format[i]) {
                case 'd':  // Signed decimal integer
                case 'i': {
                    int val = va_arg(args, int);
                    itoa(val, buffer, 10);
                    kprint(buffer);
                    break;
                }
                
                case 'u': {  // Unsigned decimal integer
                    unsigned int val = va_arg(args, unsigned int);
                    utoa(val, buffer, 10);
                    kprint(buffer);
                    break;
                }
                
                case 'x': {  // Lowercase hexadecimal
                    unsigned int val = va_arg(args, unsigned int);
                    utoa(val, buffer, 16);
                    kprint(buffer);
                    break;
                }
                
                case 'X': {  // Uppercase hexadecimal
                    unsigned int val = va_arg(args, unsigned int);
                    utoa(val, buffer, 16);
                    // Convert to uppercase
                    for (int j = 0; buffer[j]; j++) {
                        if (buffer[j] >= 'a' && buffer[j] <= 'f')
                            buffer[j] -= 32;  // Convert to uppercase
                    }
                    kprint(buffer);
                    break;
                }
                
                case 'c': {  // Character
                    // Note: char is promoted to int in varargs
                    char c = (char)va_arg(args, int);
                    char str[2] = {c, '\0'};
                    kprint(str);
                    break;
                }
                
                case 's': {  // String
                    char* str = va_arg(args, char*);
                    if (str == NULL)
                        kprint("(null)");
                    else
                        kprint(str);
                    break;
                }
                
                case 'p': {  // Pointer (hexadecimal address)
                    void* ptr = va_arg(args, void*);
                    kprint("0x");
                    utoa((unsigned int)ptr, buffer, 16);
                    kprint(buffer);
                    break;
                }
                
                case '%': {  // Literal percent sign
                    char str[2] = {'%', '\0'};
                    kprint(str);
                    break;
                }
                
                default: {  // Unknown format specifier - print as-is
                    char str[3] = {'%', format[i], '\0'};
                    kprint(str);
                    break;
                }
            }
        } else {
            // Regular character
            char str[2] = {format[i], '\0'};
            kprint(str);
        }
    }
    
    va_end(args);
}
