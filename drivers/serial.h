#ifndef SERIAL_H
#define SERIAL_H

#include "../libc/stdint.h"

// COM1 serial port base address
#define SERIAL_COM1 0x3F8

// Initialize serial port
void serial_init();

// Write a single character to serial port
void serial_putc(char c);

// Write a string to serial port
void serial_puts(const char* str);

#endif
