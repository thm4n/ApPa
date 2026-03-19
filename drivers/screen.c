#include "screen.h"
#include "serial.h"

//private functions

int get_cursor_offset();
void set_cursor_offset(int offset);
int print_char(char c, int col, int row, char attr);
int get_offset(int col, int row);
int get_offset_row(int offset);
int get_offset_col(int offset);
void scroll_screen();

//public functions:

void clear_screen() {
	int screen_size = MAX_COLS * MAX_ROWS;
	char* screen = VIDEO_ADDRESS;

	for(int i = 0; i < screen_size; i++) {
		screen[i*2] = ' ';
		screen[i*2 + 1] = WHITE_ON_BLACK;
	}
	set_cursor_offset(get_offset(0,0));
}

void kprint_at(const char* msg, int col, int row) {
	int offset;
	if (col >= 0 && row >= 0)
		offset = get_offset(col, row);
	else {
		offset = get_cursor_offset();
		row = get_offset_row(offset);
		col = get_offset_col(offset);
	}

	for (int i = 0; msg[i] != 0; i++) {
		offset = print_char(msg[i], col, row, WHITE_ON_BLACK);
		// Also output to serial port for -nographic mode
		serial_putc(msg[i]);
		row = get_offset_row(offset);
		col = get_offset_col(offset);
	}
}

void kprint(char* message) {
	kprint_at(message, -1 ,-1);
}

void kprint_hex(uint32_t value) {
	char hex_str[11];  // "0x" + 8 hex digits + null
	hex_str[0] = '0';
	hex_str[1] = 'x';
	hex_str[10] = '\0';
	
	const char hex_chars[] = "0123456789ABCDEF";
	for (int i = 9; i >= 2; i--) {
		hex_str[i] = hex_chars[value & 0xF];
		value >>= 4;
	}
	kprint(hex_str);
}

void kprint_uint(uint32_t value) {
	char num_str[11];  // Max 10 digits for 32-bit + null
	int i = 0;
	
	// Handle zero case
	if (value == 0) {
		num_str[i++] = '0';
	} else {
		// Convert number to string (reversed)
		uint32_t temp = value;
		while (temp > 0) {
			num_str[i++] = '0' + (temp % 10);
			temp /= 10;
		}
		
		// Reverse the string
		for (int j = 0; j < i / 2; j++) {
			char tmp = num_str[j];
			num_str[j] = num_str[i - 1 - j];
			num_str[i - 1 - j] = tmp;
		}
	}
	
	num_str[i] = '\0';
	kprint(num_str);
}

void kprint_backspace() {
	int offset = get_cursor_offset();
	int row = get_offset_row(offset);
	int col = get_offset_col(offset);
	
	// Move back one position if not at start of screen
	if (offset > 0) {
		offset -= 2;
		unsigned char *vidmem = (unsigned char*) VIDEO_ADDRESS;
		vidmem[offset] = ' ';  // Erase character
		vidmem[offset+1] = WHITE_ON_BLACK;
		set_cursor_offset(offset);
	}
}

int get_cursor_offset() {
	port_byte_out(REG_SCREEN_CTRL, 14);
	int offset = port_byte_in(REG_SCREEN_DATA) << 8; 
	port_byte_out(REG_SCREEN_CTRL, 15);
	offset += port_byte_in(REG_SCREEN_DATA);
	return offset * 2;
}

void set_cursor_offset(int offset) {
	offset /= 2;
	port_byte_out(REG_SCREEN_CTRL, 14);
	port_byte_out(REG_SCREEN_DATA, (unsigned char)(offset >> 8));
	port_byte_out(REG_SCREEN_CTRL, 15);
	port_byte_out(REG_SCREEN_DATA, (unsigned char)(offset & 0xff));
}

void scroll_screen() {
	unsigned char* vidmem = (unsigned char*)VIDEO_ADDRESS;
	
	// Move all lines up by one
	for (int row = 1; row < MAX_ROWS; row++) {
		for (int col = 0; col < MAX_COLS; col++) {
			int src_offset = get_offset(col, row);
			int dst_offset = get_offset(col, row - 1);
			vidmem[dst_offset] = vidmem[src_offset];
			vidmem[dst_offset + 1] = vidmem[src_offset + 1];
		}
	}
	
	// Clear the last line
	for (int col = 0; col < MAX_COLS; col++) {
		int offset = get_offset(col, MAX_ROWS - 1);
		vidmem[offset] = ' ';
		vidmem[offset + 1] = WHITE_ON_BLACK;
	}
}

int print_char(char c, int col, int row, char attr) {
	unsigned char *vidmem = (unsigned char*) VIDEO_ADDRESS;
	if (!attr) attr = WHITE_ON_BLACK;
	
	int offset;
	if (col >= 0 && row >= 0) offset = get_offset(col, row);
	else offset = get_cursor_offset();

	// Handle newline
	if (c == '\n') {
		row = get_offset_row(offset);
		offset = get_offset(0, row + 1);
	} else {
		vidmem[offset] = c;
		vidmem[offset+1] = attr;
		offset += 2;
	}
	
	// Check if we need to scroll
	if (offset >= MAX_ROWS * MAX_COLS * 2) {
		scroll_screen();
		offset = get_offset(0, MAX_ROWS - 1);
	}
	
	set_cursor_offset(offset);
	return offset;
}

int get_offset(int col, int row) {
	return 2 * (row * MAX_COLS + col); 
}

int get_offset_row(int offset) {
	return offset / (2 * MAX_COLS); 
}

int get_offset_col(int offset) { 
	return offset/2 - get_offset_row(offset)*MAX_COLS;
}
