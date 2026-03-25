unsigned char port_byte_in ( unsigned short port ) {
	unsigned char result;
	__asm__ ( " in %%dx , %%al " : "=a" ( result ) : "d" ( port ));
	return result ;
}

void port_byte_out ( unsigned short port , unsigned char data ) {
	__asm__ ( "out %%al , %%dx " : : "a" ( data ) , "d" ( port ));
}

unsigned short port_word_in ( unsigned short port ) {
	unsigned short result ;
	__asm__ ( " in %%dx , %%ax " : "=a" ( result ) : "d" ( port ));
	return result;
}

void port_word_out ( unsigned short port , unsigned short data ) {
	__asm__ ( " out %%ax , %%dx " : : "a" ( data ) , "d" ( port ));
}

void port_words_in(unsigned short port, unsigned short* buf, unsigned int count) {
	__asm__ volatile("rep insw" : "+D"(buf), "+c"(count) : "d"(port) : "memory");
}

void port_words_out(unsigned short port, const unsigned short* buf, unsigned int count) {
	__asm__ volatile("rep outsw" : "+S"(buf), "+c"(count) : "d"(port) : "memory");
}

void io_wait(void) {
	/* Write to unused port 0x80 — takes ~1 microsecond (>400ns needed by ATA) */
	__asm__ volatile("outb %%al, $0x80" : : "a"(0));
}
