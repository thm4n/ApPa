#include "shell.h"
#include "../drivers/screen.h"
#include "../libc/string.h"
#include "kmalloc.h"
#include "timer.h"

/*
 * ApPa Simple Command Shell
 * 
 * Provides a command-line interface for interacting with the kernel.
 * Supports basic commands: help, clear, echo, mem, color
 */

static char command_buffer[SHELL_BUFFER_SIZE];
static int buffer_pos = 0;

// Command handlers
static void cmd_help(void);
static void cmd_clear(void);
static void cmd_echo(const char* args);
static void cmd_mem(void);
static void cmd_color(const char* args);
static void cmd_uptime(void);

/**
 * shell_init - Initialize the shell
 */
void shell_init(void) {
	buffer_pos = 0;
	command_buffer[0] = '\0';
}

/**
 * shell_input - Process a single character from keyboard
 * @c: Character to process
 * 
 * Handles backspace, enter, and normal character input.
 */
void shell_input(char c) {
	if (c == '\b') {
		// Backspace: remove character from buffer
		if (buffer_pos > 0) {
			buffer_pos--;
			command_buffer[buffer_pos] = '\0';
			kprint_backspace();
		}
	} else if (c == '\n') {
		// Enter: execute command
		kprint("\n");
		command_buffer[buffer_pos] = '\0';
		
		// Execute if not empty
		if (buffer_pos > 0) {
			shell_execute(command_buffer);
		}
		
		// Reset buffer and print prompt
		buffer_pos = 0;
		command_buffer[0] = '\0';
		kprint("> ");
	} else {
		// Normal character: add to buffer
		if (buffer_pos < SHELL_BUFFER_SIZE - 1) {
			command_buffer[buffer_pos++] = c;
			command_buffer[buffer_pos] = '\0';
			
			// Echo character to screen
			char str[2] = {c, '\0'};
			kprint(str);
		}
	}
}

/**
 * shell_execute - Execute a command
 * @cmd: Command string to execute
 * 
 * Parses the command and calls appropriate handler.
 */
void shell_execute(const char* cmd) {
	// Skip leading spaces
	while (*cmd == ' ') cmd++;
	
	// Empty command
	if (*cmd == '\0') return;
	
	// Find first space (separates command from arguments)
	const char* args = cmd;
	while (*args && *args != ' ') args++;
	int cmd_len = args - cmd;
	
	// Skip spaces after command
	while (*args == ' ') args++;
	
	// Match command
	if (strncmp(cmd, "help", cmd_len) == 0 && cmd_len == 4) {
		cmd_help();
	} else if (strncmp(cmd, "clear", cmd_len) == 0 && cmd_len == 5) {
		cmd_clear();
	} else if (strncmp(cmd, "echo", cmd_len) == 0 && cmd_len == 4) {
		cmd_echo(args);
	} else if (strncmp(cmd, "mem", cmd_len) == 0 && cmd_len == 3) {
		cmd_mem();
	} else if (strncmp(cmd, "color", cmd_len) == 0 && cmd_len == 5) {
		cmd_color(args);
	} else if (strncmp(cmd, "uptime", cmd_len) == 0 && cmd_len == 6) {
		cmd_uptime();
	} else {
		kprint("Unknown command: ");
		kprint((char*)cmd);
		kprint("\nType 'help' for available commands.\n");
	}
}

/**
 * cmd_help - Display available commands
 */
static void cmd_help(void) {
	kprint("Available commands:\n");
	kprint("  help         - Show this help message\n");
	kprint("  clear        - Clear the screen\n");
	kprint("  echo <text>  - Print text to screen\n");
	kprint("  mem          - Display memory allocation statistics\n");
	kprint("  uptime       - Show system uptime\n");
	kprint("  color <name> - Change text color\n");
	kprint("               Colors: white, red, green, blue, yellow,\n");
	kprint("                       cyan, magenta, grey, black\n");
}

/**
 * cmd_clear - Clear screen and reset prompt
 */
static void cmd_clear(void) {
	clear_screen();
}

/**
 * cmd_echo - Print text to screen
 * @args: Text to print
 */
static void cmd_echo(const char* args) {
	if (*args) {
		kprint((char*)args);
		kprint("\n");
	}
}

/**
 * cmd_mem - Display memory allocation statistics
 */
static void cmd_mem(void) {
	kmalloc_status();
}

/**
 * cmd_uptime - Display system uptime
 */
static void cmd_uptime(void) {
	char uptime_str[32];
	get_uptime_string(uptime_str);
	kprint("System uptime: ");
	kprint(uptime_str);
	kprint("\n");
}

/**
 * parse_color_name - Convert color name to VGA color code
 * @name: Color name string
 * 
 * Returns: VGA color code, or 0xFF if invalid
 */
static uint8_t parse_color_name(const char* name) {
	if (strcmp(name, "black") == 0) return COLOR_BLACK;
	if (strcmp(name, "blue") == 0) return COLOR_BLUE;
	if (strcmp(name, "green") == 0) return COLOR_GREEN;
	if (strcmp(name, "cyan") == 0) return COLOR_CYAN;
	if (strcmp(name, "red") == 0) return COLOR_RED;
	if (strcmp(name, "magenta") == 0) return COLOR_MAGENTA;
	if (strcmp(name, "brown") == 0) return COLOR_BROWN;
	if (strcmp(name, "grey") == 0) return COLOR_LIGHT_GREY;
	if (strcmp(name, "darkgrey") == 0) return COLOR_DARK_GREY;
	if (strcmp(name, "lightblue") == 0) return COLOR_LIGHT_BLUE;
	if (strcmp(name, "lightgreen") == 0) return COLOR_LIGHT_GREEN;
	if (strcmp(name, "lightcyan") == 0) return COLOR_LIGHT_CYAN;
	if (strcmp(name, "lightred") == 0) return COLOR_LIGHT_RED;
	if (strcmp(name, "lightmagenta") == 0) return COLOR_LIGHT_MAGENTA;
	if (strcmp(name, "yellow") == 0) return COLOR_YELLOW;
	if (strcmp(name, "white") == 0) return COLOR_WHITE;
	
	return 0xFF; // Invalid
}

/**
 * cmd_color - Change text color
 * @args: Color name or "fg bg" pair
 */
static void cmd_color(const char* args) {
	if (!*args) {
		kprint("Usage: color <foreground> [background]\n");
		kprint("Example: color green black\n");
		return;
	}
	
	// Parse foreground color
	const char* fg_start = args;
	const char* fg_end = args;
	while (*fg_end && *fg_end != ' ') fg_end++;
	
	// Copy foreground color name
	char fg_name[32];
	int fg_len = fg_end - fg_start;
	if (fg_len >= 32) fg_len = 31;
	strncpy(fg_name, fg_start, fg_len);
	fg_name[fg_len] = '\0';
	
	uint8_t fg = parse_color_name(fg_name);
	if (fg == 0xFF) {
		kprint("Invalid foreground color: ");
		kprint(fg_name);
		kprint("\n");
		return;
	}
	
	// Parse background color (optional)
	uint8_t bg = COLOR_BLACK; // Default background
	const char* bg_start = fg_end;
	while (*bg_start == ' ') bg_start++;
	
	if (*bg_start) {
		const char* bg_end = bg_start;
		while (*bg_end && *bg_end != ' ') bg_end++;
		
		char bg_name[32];
		int bg_len = bg_end - bg_start;
		if (bg_len >= 32) bg_len = 31;
		strncpy(bg_name, bg_start, bg_len);
		bg_name[bg_len] = '\0';
		
		bg = parse_color_name(bg_name);
		if (bg == 0xFF) {
			kprint("Invalid background color: ");
			kprint(bg_name);
			kprint("\n");
			return;
		}
	}
	
	// Set the new color
	set_text_color(VGA_COLOR(fg, bg));
	kprint("Color changed.\n");
}
