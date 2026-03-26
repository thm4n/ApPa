#include "shell.h"
#include "../drivers/screen.h"
#include "../drivers/ata.h"
#include "../klibc/string.h"
#include "../klibc/stdio.h"
#include "../fs/simplefs.h"
#include "../kernel/mem/kmalloc.h"
#include "../kernel/mem/pmm.h"
#include "../kernel/mem/paging.h"
#include "../kernel/sys/timer.h"
#include "../kernel/sys/klog.h"
#include "../tests/test_multitask.h"
#include "../tests/test_userspace.h"
#include "../tests/test_addrspace.h"
#include "../tests/test_elf.h"
#include "../kernel/exec/elf.h"

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
static void cmd_pmem(void);
static void cmd_color(const char* args);
static void cmd_uptime(void);
static void cmd_pagedir(void);
static void cmd_ls(const char* args);
static void cmd_cat(const char* args);
static void cmd_write(const char* args);
static void cmd_mkdir(const char* args);
static void cmd_rm(const char* args);
static void cmd_disk(void);
static void cmd_tasktest(void);
static void cmd_usertest(void);
static void cmd_addrtest(void);
static void cmd_exec(const char* args);
static void cmd_elftest(void);
static void cmd_dmesg(const char* args);

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
	} else if (strncmp(cmd, "pmem", cmd_len) == 0 && cmd_len == 4) {
		cmd_pmem();
	} else if (strncmp(cmd, "color", cmd_len) == 0 && cmd_len == 5) {
		cmd_color(args);
	} else if (strncmp(cmd, "uptime", cmd_len) == 0 && cmd_len == 6) {
		cmd_uptime();
	} else if (strncmp(cmd, "pagedir", cmd_len) == 0 && cmd_len == 7) {
		cmd_pagedir();
	} else if (strncmp(cmd, "ls", cmd_len) == 0 && cmd_len == 2) {
		cmd_ls(args);
	} else if (strncmp(cmd, "cat", cmd_len) == 0 && cmd_len == 3) {
		cmd_cat(args);
	} else if (strncmp(cmd, "write", cmd_len) == 0 && cmd_len == 5) {
		cmd_write(args);
	} else if (strncmp(cmd, "mkdir", cmd_len) == 0 && cmd_len == 5) {
		cmd_mkdir(args);
	} else if (strncmp(cmd, "rm", cmd_len) == 0 && cmd_len == 2) {
		cmd_rm(args);
	} else if (strncmp(cmd, "disk", cmd_len) == 0 && cmd_len == 4) {
		cmd_disk();
	} else if (strncmp(cmd, "tasktest", cmd_len) == 0 && cmd_len == 8) {
		cmd_tasktest();
	} else if (strncmp(cmd, "usertest", cmd_len) == 0 && cmd_len == 8) {
		cmd_usertest();
	} else if (strncmp(cmd, "addrtest", cmd_len) == 0 && cmd_len == 8) {
		cmd_addrtest();
	} else if (strncmp(cmd, "exec", cmd_len) == 0 && cmd_len == 4) {
		cmd_exec(args);
	} else if (strncmp(cmd, "elftest", cmd_len) == 0 && cmd_len == 7) {
		cmd_elftest();
	} else if (strncmp(cmd, "dmesg", cmd_len) == 0 && cmd_len == 5) {
		cmd_dmesg(args);
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
	kprint("  pmem         - Display physical memory statistics\n");
	kprint("  uptime       - Show system uptime\n");
	kprint("  pagedir      - Display page directory info\n");
	kprint("  ls           - List files and directories\n");
	kprint("  cat <file>   - Display file contents\n");
	kprint("  write <file> <text> - Write text to a file\n");
	kprint("  mkdir <name> - Create a directory\n");
	kprint("  rm <name>    - Delete a file or directory\n");
	kprint("  disk         - Show ATA disk information\n");
	kprint("  tasktest     - Run multitasking tests\n");
	kprint("  usertest     - Run Ring 3 userspace tests\n");
	kprint("  addrtest     - Run per-process address space tests\n");
	kprint("  exec <file>  - Load & run ELF binary from filesystem\n");
	kprint("  elftest      - Run ELF loader tests\n");
	kprint("  dmesg        - Display kernel log buffer\n");
	kprint("  dmesg save   - Flush kernel log to klog.txt\n");
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
 * cmd_pmem - Display physical memory allocation statistics
 */
static void cmd_pmem(void) {
	pmm_status();
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
 * cmd_pagedir - Display page directory information
 */
static void cmd_pagedir(void) {
	paging_status();
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

/* ========== Phase 11: File System & Disk Commands ========== */

/**
 * cmd_ls - List files and directories
 */
static void cmd_ls(const char* args) {
	(void)args;  /* No subdirectory support in v1 */

	fs_entry_t entries[FS_MAX_ENTRIES];
	uint32_t count = fs_list(entries, FS_MAX_ENTRIES);

	if (count == 0) {
		kprint("  (empty)\n");
		return;
	}

	for (uint32_t i = 0; i < count; i++) {
		/* Type */
		if (entries[i].type == FS_TYPE_FILE) {
			kprint("  FILE  ");
		} else if (entries[i].type == FS_TYPE_DIR) {
			kprint("  DIR   ");
		} else {
			kprint("  ???   ");
		}

		/* Size (right-justify in 8 chars) */
		char size_str[16];
		uitoa(entries[i].size, size_str, 10);
		uint32_t slen = strlen(size_str);
		for (uint32_t s = slen; s < 8; s++) kprint(" ");
		kprint(size_str);
		kprint(" B   ");

		/* Name */
		kprint(entries[i].name);
		kprint("\n");
	}

	kprint("  ");
	char count_str[16];
	uitoa(count, count_str, 10);
	kprint(count_str);
	kprint(" entries\n");
}

/**
 * cmd_cat - Display file contents
 */
static void cmd_cat(const char* args) {
	if (!*args) {
		kprint("Usage: cat <filename>\n");
		return;
	}

	/* Read up to 4KB */
	static char read_buf[4096];
	int32_t bytes = fs_read_file(args, read_buf, sizeof(read_buf) - 1);

	if (bytes < 0) {
		kprint("Error: file not found '");
		kprint((char*)args);
		kprint("'\n");
		return;
	}

	read_buf[bytes] = '\0';
	kprint(read_buf);
	kprint("\n");
}

/**
 * cmd_write - Write text to a file
 * Format: write <filename> <text...>
 */
static void cmd_write(const char* args) {
	if (!*args) {
		kprint("Usage: write <filename> <text>\n");
		return;
	}

	/* Parse filename (first word) */
	const char* name_start = args;
	const char* name_end = args;
	while (*name_end && *name_end != ' ') name_end++;
	int name_len = name_end - name_start;

	if (name_len == 0 || name_len > FS_NAME_MAX) {
		kprint("Error: invalid filename\n");
		return;
	}

	char filename[24];
	strncpy(filename, name_start, name_len);
	filename[name_len] = '\0';

	/* Get text content (everything after filename) */
	const char* text = name_end;
	while (*text == ' ') text++;

	if (!*text) {
		kprint("Error: no text provided\n");
		return;
	}

	/* Create file if it doesn't exist */
	fs_entry_t stat;
	if (fs_stat(filename, &stat) != 0) {
		if (fs_create(filename, FS_TYPE_FILE) != 0) {
			kprint("Error: could not create file\n");
			return;
		}
	}

	/* Write data */
	uint32_t text_len = strlen(text);
	if (fs_write_file(filename, text, text_len) != 0) {
		kprint("Error: write failed\n");
		return;
	}

	kprint("Written ");
	char len_str[16];
	uitoa(text_len, len_str, 10);
	kprint(len_str);
	kprint(" bytes to '");
	kprint(filename);
	kprint("'\n");
}

/**
 * cmd_mkdir - Create a directory
 */
static void cmd_mkdir(const char* args) {
	if (!*args) {
		kprint("Usage: mkdir <name>\n");
		return;
	}

	if (fs_create(args, FS_TYPE_DIR) != 0) {
		kprint("Error: could not create directory '");
		kprint((char*)args);
		kprint("' (exists or full)\n");
		return;
	}

	kprint("Created directory '");
	kprint((char*)args);
	kprint("'\n");
}

/**
 * cmd_rm - Delete a file or directory
 */
static void cmd_rm(const char* args) {
	if (!*args) {
		kprint("Usage: rm <name>\n");
		return;
	}

	if (fs_delete(args) != 0) {
		kprint("Error: '");
		kprint((char*)args);
		kprint("' not found\n");
		return;
	}

	kprint("Deleted '");
	kprint((char*)args);
	kprint("'\n");
}

/**
 * cmd_disk - Show ATA disk information
 */
static void cmd_disk(void) {
	ata_status();
}

/**
 * cmd_tasktest - Run multitasking tests
 * Spawns test tasks and verifies context switching, scheduling,
 * task lifecycle, and interleaving.
 */
static void cmd_tasktest(void) {
	test_multitask();
}

/**
 * cmd_usertest - Run Phase 13 userspace tests
 * Spawns Ring 3 tasks and verifies syscalls (write, getpid, yield, exit)
 * and GPF isolation (privileged instruction in user mode → killed).
 */
static void cmd_usertest(void) {
	test_userspace();
}

/**
 * cmd_addrtest - Run Phase 15 per-process address space tests
 * Spawns Ring 3 tasks with private page directories and verifies
 * memory isolation, syscall operation, and cleanup.
 */
static void cmd_addrtest(void) {
	test_addrspace();
}

/**
 * cmd_exec - Load and execute an ELF binary from the filesystem
 * @args: Filename followed by optional arguments (e.g. "hello.elf foo bar")
 */
static void cmd_exec(const char* args) {
	if (!*args) {
		kprint("Usage: exec <filename> [args...]\n");
		return;
	}

	/* Tokenize into argv array (max 16 args) */
	#define MAX_EXEC_ARGS 16
	static char arg_buf[256];
	const char *argv[MAX_EXEC_ARGS + 1];
	int argc = 0;

	/* Copy args to mutable buffer */
	int len = 0;
	while (args[len] && len < 255) { arg_buf[len] = args[len]; len++; }
	arg_buf[len] = '\0';

	/* Split by spaces */
	char *p = arg_buf;
	while (*p && argc < MAX_EXEC_ARGS) {
		/* Skip leading spaces */
		while (*p == ' ') p++;
		if (!*p) break;
		argv[argc++] = p;
		/* Find end of token */
		while (*p && *p != ' ') p++;
		if (*p) *p++ = '\0';
	}
	argv[argc] = 0;  /* NULL sentinel */

	if (argc == 0) {
		kprint("Usage: exec <filename> [args...]\n");
		return;
	}

	kprintf("[ELF] Loading '%s' with %d arg(s)...\n", argv[0], argc);

	task_t *t = elf_exec(argv[0], argv[0], argv, argc);
	if (!t) {
		kprintf("[ELF] Failed to load '%s'\n", argv[0]);
		return;
	}

	kprintf("[ELF] Spawned task '%s' (tid=%d)\n", argv[0], t->id);
}

/**
 * cmd_elftest - Run ELF loader unit tests
 */
static void cmd_elftest(void) {
	test_elf();
}

/**
 * cmd_dmesg - Display or save kernel log
 * @args: "" to display, "save" to flush to klog.txt
 */
static void cmd_dmesg(const char* args) {
	if (*args && strcmp(args, "save") == 0) {
		if (klog_flush_to_file("klog.txt") == 0) {
			kprint("Kernel log saved to 'klog.txt'\n");
		} else {
			kprint("Error: could not save kernel log\n");
		}
	} else if (*args && strcmp(args, "clear") == 0) {
		klog_clear();
	} else {
		klog_dump();
	}
}
