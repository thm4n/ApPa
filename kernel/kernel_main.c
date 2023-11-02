#include "../drivers/screen.h"

void __stack_chk_fail() {}

int offsetToCols(int offset);
int offsetToRows(int offset);

int CoordsToOffset(int x, int y);

int putchar_xy(char ch, int x, int y, char attr);
//sint putchar_offset(char ch, int offset, char attr);

void main() {
	clear_screen();

	char str[] = "1234";
	int offset = get_offset(0,0);
	for (int i = 0; str[i] != 0; i++) {
		offset = print_char(str[i], offset, GREY_ON_BLACK);
	}
}
