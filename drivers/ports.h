unsigned char port_byte_in(unsigned short port);
void port_byte_out(unsigned short port, unsigned char data);
unsigned short port_word_in(unsigned short port);
void port_word_out(unsigned short port, unsigned short data);
void port_words_in(unsigned short port, unsigned short* buf, unsigned int count);
void port_words_out(unsigned short port, const unsigned short* buf, unsigned int count);
void io_wait(void);
