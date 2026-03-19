#ifndef SHELL_H
#define SHELL_H

#include "../libc/stdint.h"

#define SHELL_BUFFER_SIZE 256

// Shell initialization
void shell_init(void);

// Process a single character from keyboard
void shell_input(char c);

// Execute a command
void shell_execute(const char* cmd);

#endif
