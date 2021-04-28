#include "../drivers/screen.h"

void main() {
	char string1[] = "abcd";
	char string2[] = "1234";
	char string3[] = "zxcv";
	char string4[] = "asdf";
	clean_screen();
	kprint_at(string1, 1, 1);
	kprint_at(string2, 1, 2);
	//kprint_at(string3, 0, 20);
	//kprint(strings[3]);
}
