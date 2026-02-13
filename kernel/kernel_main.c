#include "../drivers/screen.h"

void __stack_chk_fail() {}

int CoordsToOffset(int x, int y);

int putchar_xy(char ch, int x, int y, char attr);
//sint putchar_offset(char ch, int offset, char attr);

void main() {
	clear_screen();

	char str[] = "1234";
	int offset = get_offset(0,0);
	for (int i = 0; str[i] != 0; i++) {
		offset = print_char(str[i], get_offset_col(offset), get_offset_row(offset), GREY_ON_BLACK);
	}
}
