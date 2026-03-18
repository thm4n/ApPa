# $@ = target file
# $< = first dependcy
# $^ = all dependecies

# Disable built-in implicit rules
MAKEFLAGS += --no-builtin-rules
.SUFFIXES:

C_SOURCES = $(wildcard *.c */*.c)
HEADERS = $(wildcard *.h */*.h)
OBJ = ${C_SOURCES:.c=.o}

# Kernel assembly sources (not boot sector)
KERNEL_ASM_SOURCES = $(wildcard kernel/*.asm)
KERNEL_ASM_OBJ = ${KERNEL_ASM_SOURCES:.asm=.o}

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
$(info KERNEL_ASM_OBJ = $(KERNEL_ASM_OBJ))
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

# Sector count patch offset in boot_sector.bin (location of 'mov dh, X' immediate)
# Use: nasm -f bin boot/boot_sector.asm -o /dev/null -l /dev/stdout | grep KERNEL_SECTORS_PATCH
# to recalculate if boot sector changes significantly
SECTOR_PATCH_OFFSET = 0x142

$(BIN_DIR)/image.bin: $(BIN_DIR)/boot_sector.bin $(BIN_DIR)/kernel.bin | $(BIN_DIR)
	@echo "[DEBUG] Building target: $@"
	@# Calculate required sectors (rounded up)
	@KERNEL_SIZE=$$(stat -c%s $(BIN_DIR)/kernel.bin); \
	SECTORS=$$(( ($$KERNEL_SIZE + 511) / 512 )); \
	if [ $$SECTORS -gt 63 ]; then \
		echo "ERROR: kernel.bin ($$KERNEL_SIZE bytes, $$SECTORS sectors) exceeds 63 sectors (32KB)"; \
		echo "You need to implement a two-stage bootloader. See README.md"; \
		exit 1; \
	fi; \
	echo "[DEBUG] Kernel size: $$KERNEL_SIZE bytes, requires $$SECTORS sectors"; \
	python3 -c "import sys; sys.stdout.buffer.write(bytes([$$SECTORS]))" | dd of=$(BIN_DIR)/boot_sector.bin bs=1 seek=$$(($(SECTOR_PATCH_OFFSET))) conv=notrunc status=none; \
	echo "[DEBUG] Patched boot sector to load $$SECTORS sectors"; \
	cat $(BIN_DIR)/boot_sector.bin $(BIN_DIR)/kernel.bin > $@; \
	IMAGE_SIZE=$$((512 + $$SECTORS * 512)); \
	dd if=/dev/zero of=$@ bs=1 count=0 seek=$$IMAGE_SIZE 2>/dev/null; \
	echo "[DEBUG] Created $@ successfully ($$IMAGE_SIZE bytes)"

$(BIN_DIR)/kernel.bin: boot/kernel_entry.o ${OBJ} ${KERNEL_ASM_OBJ} | $(BIN_DIR)
	@echo "[DEBUG] Building target: $@"
	@echo "[DEBUG] Linking objects: $^"
	@echo "[DEBUG] Command: ${LD} ${LDFLAGS} -o $@ -Ttext 0x1000 $^ --oformat binary"
	${LD} ${LDFLAGS} -o $@ -Ttext 0x1000 $^ --oformat binary
	@echo "[DEBUG] Created $@ successfully"

$(BIN_DIR)/kernel.elf: boot/kernel_entry.o ${OBJ} ${KERNEL_ASM_OBJ} | $(BIN_DIR)
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
