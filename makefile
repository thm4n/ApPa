# $@ = target file
# $< = first dependcy
# $^ = all dependecies

# Disable built-in implicit rules
MAKEFLAGS += --no-builtin-rules
.SUFFIXES:

C_SOURCES = $(wildcard *.c */*.c)
HEADERS = $(wildcard *.h */*.h)
OBJ = ${C_SOURCES:.c=.o}

BIN_DIR = bin

CROSS_COMPILER_BINS = /home/thman/.Code/CrossCompiler/i686_elf/bin/i686-elf-
CC = ${CROSS_COMPILER_BINS}gcc
LD = ${CROSS_COMPILER_BINS}ld
SM = nasm
GDB = ${CROSS_COMPILER_BINS}gdb

CCFLAGS = -g -fno-stack-protector -march=i686
LDFLAGS = -g
SMFLAGS = 

# Debug: Print variables at parse time
$(info ========== MAKEFILE DEBUG INFO ==========)
$(info C_SOURCES = $(C_SOURCES))
$(info HEADERS   = $(HEADERS))
$(info OBJ       = $(OBJ))
$(info CC        = $(CC))
$(info LD        = $(LD))
$(info CCFLAGS   = $(CCFLAGS))
$(info LDFLAGS   = $(LDFLAGS))
$(info ==========================================)

# Ensure bin directory exists
$(BIN_DIR):
	@mkdir -p $(BIN_DIR)

check:
	@echo "[DEBUG] check: Listing C sources"
	echo ${C_SOURCES}

$(BIN_DIR)/image.bin: $(BIN_DIR)/boot_sector.bin $(BIN_DIR)/kernel.bin | $(BIN_DIR)
	@echo "[DEBUG] Building target: $@"
	@echo "[DEBUG] Dependencies: $^"
	cat $^ > $@
	@# Pad the image to ensure it has enough sectors for BIOS to read
	@# Boot sector reads 4 sectors (2048 bytes) for the kernel
	dd if=/dev/zero of=$@ bs=1 count=0 seek=2560 2>/dev/null
	@echo "[DEBUG] Created $@ successfully (padded to 2560 bytes)"

$(BIN_DIR)/kernel.bin: boot/kernel_entry.o ${OBJ} | $(BIN_DIR)
	@echo "[DEBUG] Building target: $@"
	@echo "[DEBUG] Linking objects: $^"
	@echo "[DEBUG] Command: ${LD} ${LDFLAGS} -o $@ -Ttext 0x1000 $^ --oformat binary"
	${LD} ${LDFLAGS} -o $@ -Ttext 0x1000 $^ --oformat binary
	@echo "[DEBUG] Created $@ successfully"

$(BIN_DIR)/kernel.elf: boot/kernel_entry.o ${OBJ} | $(BIN_DIR)
	@echo "[DEBUG] Building target: $@"
	@echo "[DEBUG] Linking objects: $^"
	@echo "[DEBUG] Command: ${LD} ${LDFLAGS} -o $@ -Ttext 0x1000 $^"
	${LD} ${LDFLAGS} -o $@ -Ttext 0x1000 $^
	@echo "[DEBUG] Created $@ successfully"

run: $(BIN_DIR)/image.bin 
	@echo "[DEBUG] Running QEMU with image: $<"
	qemu-system-i386 -s -drive file=$<,format=raw

debug: $(BIN_DIR)/image.bin $(BIN_DIR)/kernel.elf
	@echo "[DEBUG] Starting debug session"
	@echo "[DEBUG] Launching QEMU in background with image: $<"
	qemu-system-i386 -s -drive file=$<,format=raw &
	@echo "[DEBUG] Connecting GDB to localhost:1234"
	${GDB} -ex "target remote localhost:1234" -ex "symbol-file $(BIN_DIR)/kernel.elf"

%.o: %.c ${HEADERS}
	@echo "[DEBUG] Compiling C file: $< -> $@"
	@echo "[DEBUG] Command: ${CC} ${CCFLAGS} -ffreestanding -c $< -o $@"
	${CC} ${CCFLAGS} -ffreestanding -c $< -o $@
	@echo "[DEBUG] Compiled $@ successfully"

%.o: %.asm
	@echo "[DEBUG] Assembling ASM file: $< -> $@"
	@echo "[DEBUG] Command: nasm ${SMFLAGS} $< -f elf32 -o $@"
	nasm ${SMFLAGS} $< -f elf32 -o $@
	@echo "[DEBUG] Assembled $@ successfully"

$(BIN_DIR)/%.bin: boot/%.asm | $(BIN_DIR)
	@echo "[DEBUG] Assembling ASM to binary: $< -> $@"
	@echo "[DEBUG] Command: nasm ${SMFLAGS} $< -f bin -o $@"
	nasm ${SMFLAGS} $< -f bin -o $@
	@echo "[DEBUG] Created $@ successfully"

clean:
	@echo "[DEBUG] Cleaning build artifacts..."
	@echo "[DEBUG] Removing: $(BIN_DIR)/*"
	rm -rf $(BIN_DIR)/*
	@echo "[DEBUG] Removing: $(wildcard *.o */*.o)"
	rm -f $(wildcard *.o */*.o)
	@echo "[DEBUG] Clean complete"
	@echo "[DEBUG] Clean complete"