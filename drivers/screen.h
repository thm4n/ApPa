#include "ports.h"
#include "../klibc/stdint.h"

#define VIDEO_ADDRESS (unsigned char*)0xb8000
#define MAX_ROWS 25
#define MAX_COLS 80

#define GREY_ON_BLACK 0x07
#define WHITE_ON_BLACK 0x0f
#define RED_ON_WHITE 0xf4

// VGA Color Constants
#define COLOR_BLACK 0x0
#define COLOR_BLUE 0x1
#define COLOR_GREEN 0x2
#define COLOR_CYAN 0x3
#define COLOR_RED 0x4
#define COLOR_MAGENTA 0x5
#define COLOR_BROWN 0x6
#define COLOR_LIGHT_GREY 0x7
#define COLOR_DARK_GREY 0x8
#define COLOR_LIGHT_BLUE 0x9
#define COLOR_LIGHT_GREEN 0xA
#define COLOR_LIGHT_CYAN 0xB
#define COLOR_LIGHT_RED 0xC
#define COLOR_LIGHT_MAGENTA 0xD
#define COLOR_YELLOW 0xE
#define COLOR_WHITE 0xF

// Helper macro to create color byte: background (high nibble) | foreground (low nibble)
#define VGA_COLOR(fg, bg) ((bg << 4) | fg)

#define REG_SCREEN_CTRL 0x3d4
#define REG_SCREEN_DATA 0x3d5

void clear_screen();
void kprint_at(const char* msg, int col, int row);
void kprint(char* message);
void kprint_hex(uint32_t value);
void kprint_uint(uint32_t value);
void kprint_backspace();
void scroll_screen();
void set_text_color(char color);
char get_text_color();

int get_offset(int col, int row);
int get_offset_row(int offset);
int get_offset_col(int offset);

int print_char(char c, int col, int row, char attr);
