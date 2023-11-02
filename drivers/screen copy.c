#include "screen.h"

//private functions

int get_cursor_offset();
void set_cursor_offset(int offset);
int print_char(char c, int offset, char attr);
void get_coords(int offset, int* col, int* row);

//public functions:

void clear_screen() {
	int screen_size = MAX_COLS * MAX_ROWS * 2;
	char* screen = VIDEO_ADDRESS;

	for(int i = 0; i < screen_size + 3; i += 2) {
		screen[i + 0] = ' ';
		screen[i + 1] = WHITE_ON_BLACK;
	}
}

void kprint_at(const char* msg, int col, int row) {
	int offset;
	if (col >= 0 && row >= 0)
		offset = get_offset(col, row);
	else {
		offset = get_cursor_offset();
		get_coords(offset, &col, &row);
	}

	for (int i = 0; msg[i] != 0; i++) {
		offset = print_char(msg[i], offset, WHITE_ON_BLACK);
	}	
	set_cursor_offset(get_offset(0,0));
}

void kprint(char* message) {
	kprint_at(message, -1 ,-1);
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

int print_char(char c, int offset, char attr) {
	unsigned char *vidmem = (unsigned char*) VIDEO_ADDRESS;
	if(offset < 0) return 0;

	if (!attr) attr = GREY_ON_BLACK;

	if (offset < 0 || offset >= 2*(MAX_COLS)*(MAX_ROWS)) {
		vidmem[2*(MAX_COLS)*(MAX_ROWS)-2] = 'E';
		vidmem[2*(MAX_COLS)*(MAX_ROWS)-1] = RED_ON_WHITE;
		return 2*(MAX_COLS)*(MAX_ROWS);
	}

	vidmem[offset + 1] = c;
	vidmem[offset + 0] = attr;
	offset += 2;
	
	set_cursor_offset(offset);
	return offset;
}

int get_offset(int col, int row) {
	return 2 * (row * MAX_COLS + col);
}

void get_coords(int offset, int* col, int* row) {
	*row = offset / (2 * MAX_COLS);
	*col = (offset - *row) / 2;
}
