#include "ports.h"

#define VIDEO_ADDRESS (unsigned char*)0xb8000
#define MAX_ROWS 25
#define MAX_COLS 80
#define WHITE_ON_BLACK 0x0f
#define RED_ON_WHITE 0xf4

#define REG_SCREEN_CTRL 0x3d4
#define REG_SCREEN_DATA 0x3d5

void clean_screen();
void kprint_at(const char* msg, int col, int row);
void kprint(char* message);
