# $@ = target file
# $< = first dependcy
# $^ = all dependecies

C_SOURCES = $(wildcard *.c */*.c)
HEADERS = $(wildcard *.h */*.h)
OBJ = ${C_SOURCES:.c=.o}

CC = gcc 
LD = /home/thman/opt/cross/x86_64-elf/bin/ld
SM = nasm
GDB = gdb

CCFLAGS = -g -fno-stack-protector -march=x86-64
LDFLAGS = -g -m elf_x86_64
SMFLAGS = 

check:
	echo ${C_SOURCES}

image.bin: boot_sector.bin kernel.bin
	cat $^ > $@

kernel.bin: boot/kernel_entry.o ${OBJ}
	${LD} ${LDFLAGS} -o $@ -Ttext 0x1000 $^ --oformat binary

kernel.elf: boot/kernel_entry.o ${OBJ}
	${LD} ${LDFLAGS} -o $@ -Ttext 0x1000 $^

run: image.bin 
	qemu-system-x86_64 -s -drive file=$<,format=raw

debug: image.bin kernel.elf
	qemu-system-x86_64 -s -drive file=$<,format=raw &
	${GDB} -ex "target remote localhost:1234" -ex "symbol-file kernel.elf"

%.o: */%.c ${HEADERS}
	${CC} ${CCFLAGS} -ffreestanding -c $< -o $@

%.o: %.asm
	nasm ${SMFLAGS} $< -f elf64 -o $@

%.bin: */%.asm
	nasm ${SMFLAGS} $< -f bin -o $@

clean:
	rm $(wildcard *.bin */*.bin)
	rm $(wildcard *.o */*.o)
