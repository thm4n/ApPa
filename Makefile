# $@ = target file
# $< = first dependcy
# $^ = all dependecies

C_SOURCES = $(wildcard kernel/*.c drivers/*.c)
HEADERS = $(wildcard kernel/*.h drivers/*.h)
OBJ = ${C_SOURCES:.c=.o}

CC = /usr/local/cross/bin/i686-elf-gcc
LD = /usr/local/cross/bin/i686-elf-ld
GDB = gdb

CFLAGS = -g

image.bin: boot_sector.bin kernel.bin
	cat $^ > $@

kernel.bin: boot/kernel_entry.o ${OBJ}
	${LD} -o $@ -Ttext 0x1000 $^ --oformat binary

kernel.elf: boot/kernel_entry.o ${OBJ}
	${LD} -g -o $@ -Ttext 0x1000 $^

run: image.bin 
	qemu-system-x86_64 -fda $<

debug: image.bin kernel.elf
	qemu-system-x86_64 -s -fda image.bin &
	${GDB} -ex "target remote localhost:1234" -ex "symbol-file kernel.elf"

%.o: */%.c ${HEADERS}
	${CC} ${FLAGS} -ffreestanding -c $< -o $@

%.o: %.asm
	nasm $< -f elf -o $@

%.bin: */%.asm
	nasm $< -f bin -o $@

clean:
	rm -rf *.bin *.dis *.o *.elf
	rm kernel/*.bin kernel/*.o boot/*.bin boot/*.o
	rm drivers/*.bin drivers/*.o
	rm image.bin
