# $@ = target file
# $< = first dependency
# $^ = all dependencies

# Disable built-in implicit rules
MAKEFLAGS += --no-builtin-rules
.SUFFIXES:

C_SOURCES = $(wildcard *.c */*.c */*/*.c)
HEADERS = $(wildcard *.h */*.h */*/*.h)
OBJ = ${C_SOURCES:.c=.o}

# Kernel assembly sources (not boot sector)
KERNEL_ASM_SOURCES = $(wildcard kernel/arch/*.asm) $(wildcard kernel/task/*.asm)
KERNEL_ASM_OBJ = ${KERNEL_ASM_SOURCES:.asm=.o}

BIN_DIR = bin

CROSS_COMPILER_BINS = /home/thman/.Code/CrossCompiler/i686_elf/bin/i686-elf-
CC = ${CROSS_COMPILER_BINS}gcc
LD = ${CROSS_COMPILER_BINS}ld
SM = nasm
GDB = ${CROSS_COMPILER_BINS}gdb

# ========== Memory Layout Configuration ==========
# These constants define the memory layout for the bootloader and kernel.
# They are passed to NASM during assembly via -D flags.
#
# Memory Map:
#   0x00000000 - 0x000007FF : Interrupt Vector Table (IVT)
#   0x00000800 - 0x00000FFF : BIOS Data Area
#   0x00001000 - 0x0000XXXX : Kernel Code (loaded here)
#   0x0000XXXX - 0x0009FBFF : Kernel Stack (grows downward from STACK_BASE)
#   0x0009FC00 - 0x0009FFFF : Stack Base (ESP starts here)
#   0x000A0000 - 0x000BFFFF : VGA Video Memory
#   0x000C0000 - 0x000FFFFF : BIOS ROM
#
# KERNEL_OFFSET: Where the kernel code is loaded (must match linker -Ttext)
KERNEL_OFFSET = 0x1000

# STACK_BASE: Top of stack in protected mode (ESP/EBP initialized here)
# Stack grows DOWN from this address toward KERNEL_OFFSET
# Current: 0x9FC00 (639KB) provides ~607KB stack space
# Maximum: 0x9FFF0 (just below VGA at 0xA0000)
STACK_BASE = 0x9FC00

# REAL_MODE_STACK: Stack location in 16-bit real mode (bootloader phase)
# Lower value since we're still in 16-bit mode with limited addressing
REAL_MODE_STACK = 0x9000
# ==================================================

CCFLAGS = -g -fno-stack-protector -march=i686
LDFLAGS = -g
SMFLAGS = -D KERNEL_OFFSET=$(KERNEL_OFFSET) -D STACK_BASE=$(STACK_BASE) -D REAL_MODE_STACK=$(REAL_MODE_STACK)

# ========== Output Formatting ==========
# V=1  on the command line enables verbose mode (shows raw commands)
V ?= 0

# ANSI color codes
CLR_RST  = \033[0m
CLR_BLD  = \033[1m
CLR_RED  = \033[1;31m
CLR_GRN  = \033[1;32m
CLR_YLW  = \033[1;33m
CLR_BLU  = \033[1;34m
CLR_CYN  = \033[1;36m
CLR_DIM  = \033[0;37m

# Pretty-print helpers: tag in color, path dimmed
# Usage: $(call log,TAG,message)
define log
	@printf "  $(CLR_CYN)%-8s$(CLR_RST) %s\n" "[$(1)]" "$(2)"
endef

# Conditional command echo: silent by default, verbose with V=1
ifeq ($(V),1)
  Q =
  QLOG = @true
else
  Q = @
  QLOG = @
endif

# Count sources for the summary
N_C_SRC   := $(words $(C_SOURCES))
N_ASM_SRC := $(words $(KERNEL_ASM_SOURCES))

# =======================================

# Default target — show available commands
all: help

# ========== Help ==========
help:
	@printf "\n$(CLR_BLD)  ApPa OS$(CLR_RST)$(CLR_DIM)  ──  i686 bare-metal kernel$(CLR_RST)\n"
	@printf "$(CLR_DIM)  ─────────────────────────────────────────$(CLR_RST)\n"
	@printf "  $(CLR_CYN)%-20s$(CLR_RST) %s\n" "make build"       "Compile kernel + assemble disk image"
	@printf "  $(CLR_CYN)%-20s$(CLR_RST) %s\n" "make run"         "Build & launch QEMU (graphical window)"
	@printf "  $(CLR_CYN)%-20s$(CLR_RST) %s\n" "make run-term"    "Build & launch QEMU (curses terminal)"
	@printf "  $(CLR_CYN)%-20s$(CLR_RST) %s\n" "make run-log"     "Build & launch QEMU (serial → stdout)"
	@printf "  $(CLR_CYN)%-20s$(CLR_RST) %s\n" "make debug"       "Build & launch QEMU + GDB session"
	@printf "  $(CLR_CYN)%-20s$(CLR_RST) %s\n" "make clean"       "Remove build artifacts (keeps disk.img)"
	@printf "  $(CLR_CYN)%-20s$(CLR_RST) %s\n" "make disk-reset"  "Delete persistent disk image"
	@printf "  $(CLR_CYN)%-20s$(CLR_RST) %s\n" "make mem_show"    "Show memory layout of last built image"
	@printf "  $(CLR_CYN)%-20s$(CLR_RST) %s\n" "make mem_img_show" "Inspect SimpleFS disk image contents"
	@printf "  $(CLR_CYN)%-20s$(CLR_RST) %s\n" "make check"       "List all C source files"
	@printf "\n"
	@printf "  $(CLR_DIM)Options:  V=1          verbose mode (shows raw commands)$(CLR_RST)\n"
	@printf "  $(CLR_DIM)          IMG=<path>   disk image for mem_img_show (default: bin/disk.img)$(CLR_RST)\n"
	@printf "\n"

# ========== Persistent Disk Image ==========
# SimpleFS lives on a separate ATA slave disk (QEMU -hdb).
# Created once; survives 'make clean' so data persists across builds.
DISK_IMG = $(BIN_DIR)/disk.img
DISK_SIZE = 4M

$(DISK_IMG): | $(BIN_DIR)
	$(call log,DISK,Creating $(DISK_SIZE) persistent disk image)
	$(Q)qemu-img create -f raw $(DISK_IMG) $(DISK_SIZE) >/dev/null 2>&1

# Convenience target to wipe the filesystem and start fresh
disk-reset:
	$(call log,DISK,Deleting disk image (will be recreated on next run))
	@rm -f $(DISK_IMG)

# Build only (no QEMU)
build: _build_banner $(BIN_DIR)/image.bin _build_done

# Internal: print banner before build
_build_banner:
	@printf "\n$(CLR_BLD)  ApPa OS$(CLR_RST)$(CLR_DIM)  ──  i686 bare-metal kernel$(CLR_RST)\n"
	@printf "$(CLR_DIM)  ─────────────────────────────────────────$(CLR_RST)\n"
ifeq ($(V),1)
	@printf "  $(CLR_DIM)CC      = $(CC)$(CLR_RST)\n"
	@printf "  $(CLR_DIM)LD      = $(LD)$(CLR_RST)\n"
	@printf "  $(CLR_DIM)CCFLAGS = $(CCFLAGS)$(CLR_RST)\n"
	@printf "  $(CLR_DIM)Sources : $(N_C_SRC) C, $(N_ASM_SRC) ASM$(CLR_RST)\n"
	@printf "$(CLR_DIM)  ─────────────────────────────────────────$(CLR_RST)\n"
endif

# Internal: print summary after successful build
_build_done: $(BIN_DIR)/image.bin
	@KSIZE=$$(stat -c%s $(BIN_DIR)/kernel.bin 2>/dev/null || echo 0); \
	ISIZE=$$(stat -c%s $(BIN_DIR)/image.bin 2>/dev/null || echo 0); \
	printf "$(CLR_DIM)  ─────────────────────────────────────────$(CLR_RST)\n"; \
	printf "  $(CLR_GRN)%-8s$(CLR_RST) kernel %s bytes  |  image %s bytes\n" "[DONE]" "$$KSIZE" "$$ISIZE"; \
	printf "  $(CLR_DIM)%-8s $(N_C_SRC) C + $(N_ASM_SRC) ASM sources compiled$(CLR_RST)\n" ""; \
	printf "\n"

# Ensure bin directory exists
$(BIN_DIR):
	@mkdir -p $(BIN_DIR)

check:
	@printf "  $(CLR_CYN)%-8s$(CLR_RST) %s\n" "[INFO]" "C sources:"
	@echo ${C_SOURCES} | tr ' ' '\n' | sed 's/^/           /'

# Stage2 patch offset (location of sectors_remaining immediate value in stage2.asm)
# Use: nasm boot/stage2.asm -o /tmp/s2.bin -l /tmp/s2.lst && grep -A1 KERNEL_SECTORS_STAGE2_PATCH /tmp/s2.lst
STAGE2_PATCH_OFFSET = 0x3F

$(BIN_DIR)/stage2.bin: boot/stage2.asm | $(BIN_DIR)
	$(call log,ASM,$< -> $@)
	$(Q)nasm -D KERNEL_OFFSET=$(KERNEL_OFFSET) -D STACK_BASE=$(STACK_BASE) boot/stage2.asm -f bin -o $@

$(BIN_DIR)/image.bin: $(BIN_DIR)/boot_sector.bin $(BIN_DIR)/stage2.bin $(BIN_DIR)/kernel.bin | $(BIN_DIR)
	$(call log,IMG,Assembling disk image)
	@KERNEL_SIZE=$$(stat -c%s $(BIN_DIR)/kernel.bin); \
	SECTORS=$$(( ($$KERNEL_SIZE + 511) / 512 )); \
	printf "  $(CLR_DIM)%-8s kernel: $$KERNEL_SIZE bytes  ($$SECTORS sectors)$(CLR_RST)\n" ""; \
	python3 -c "import sys; sys.stdout.buffer.write(bytes([$$SECTORS]))" | dd of=$(BIN_DIR)/stage2.bin bs=1 seek=$$(($(STAGE2_PATCH_OFFSET))) conv=notrunc status=none; \
	cat $(BIN_DIR)/boot_sector.bin $(BIN_DIR)/stage2.bin $(BIN_DIR)/kernel.bin > $@; \
	IMAGE_SIZE=$$((512 + 2048 + $$SECTORS * 512)); \
	dd if=/dev/zero of=$@ bs=1 count=0 seek=$$IMAGE_SIZE 2>/dev/null; \
	printf "  $(CLR_DIM)%-8s image:  $$IMAGE_SIZE bytes  (boot 512 + stage2 2K + kernel $$KERNEL_SIZE)$(CLR_RST)\n" ""

$(BIN_DIR)/kernel.bin: boot/kernel_entry.o ${OBJ} ${KERNEL_ASM_OBJ} | $(BIN_DIR)
	$(call log,LD,kernel.bin  ($(words $^) objects))
	$(Q)${LD} ${LDFLAGS} -o $@ -Ttext 0x1000 $^ --oformat binary

$(BIN_DIR)/kernel.elf: boot/kernel_entry.o ${OBJ} ${KERNEL_ASM_OBJ} | $(BIN_DIR)
	$(call log,LD,kernel.elf  ($(words $^) objects))
	$(Q)${LD} ${LDFLAGS} -o $@ -Ttext 0x1000 $^

# Persistent disk: -drive index=1 maps to primary slave (same I/O ports, different drive select)
QEMU_DISK = -drive file=$(DISK_IMG),format=raw,index=1

# Run QEMU with graphical display window
run-graphics: $(BIN_DIR)/image.bin $(DISK_IMG)
	$(call log,QEMU,graphics mode (with persistent disk))
	@qemu-system-i386 -s -drive file=$<,format=raw $(QEMU_DISK)

# Run QEMU with curses display (terminal-based VGA with PS/2 keyboard)
run-term: $(BIN_DIR)/image.bin $(DISK_IMG)
	$(call log,QEMU,terminal curses mode (with persistent disk))
	@printf "  $(CLR_DIM)%-8s ESC+2 for monitor | Ctrl+A X to exit$(CLR_RST)\n" ""
	@qemu-system-i386 -s -drive file=$<,format=raw $(QEMU_DISK) -display curses

# Run QEMU with serial output tee'd to stdout (no keyboard input)
run-log: $(BIN_DIR)/image.bin $(DISK_IMG)
	$(call log,QEMU,serial logging mode (with persistent disk))
	@printf "  $(CLR_DIM)%-8s Output: stdout + last_run.log | Ctrl+A X to exit$(CLR_RST)\n" ""
	@rm -f last_run.log
	@qemu-system-i386 -s -drive file=$<,format=raw $(QEMU_DISK) -serial mon:stdio -nographic 2>&1 | tee last_run.log

# Aliases for convenience
run: run-graphics

debug: $(BIN_DIR)/image.bin $(BIN_DIR)/kernel.elf $(DISK_IMG)
	$(call log,DEBUG,Launching QEMU + GDB session)
	@qemu-system-i386 -s -drive file=$<,format=raw $(QEMU_DISK) &
	@printf "  $(CLR_DIM)%-8s Connecting GDB to localhost:1234$(CLR_RST)\n" ""
	@${GDB} -ex "target remote localhost:1234" -ex "symbol-file $(BIN_DIR)/kernel.elf"

%.o: %.c ${HEADERS}
	$(call log,CC,$<)
	$(Q)${CC} ${CCFLAGS} -ffreestanding -c $< -o $@

%.o: %.asm
	$(call log,ASM,$<)
	$(Q)nasm ${SMFLAGS} $< -f elf32 -o $@

$(BIN_DIR)/%.bin: boot/%.asm | $(BIN_DIR)
	$(call log,ASM,$< -> $@)
	$(Q)nasm ${SMFLAGS} $< -f bin -o $@

clean:
	@printf "\n$(CLR_BLD)  ApPa OS$(CLR_RST)$(CLR_DIM)  ──  clean$(CLR_RST)\n"
	@printf "$(CLR_DIM)  ─────────────────────────────────────────$(CLR_RST)\n"
	$(call log,CLEAN,bin/* (preserving disk.img))
	@find $(BIN_DIR) -maxdepth 1 -type f ! -name 'disk.img' -exec rm -f {} + 2>/dev/null || true
	$(call log,CLEAN,*.o)
	@rm -f $(wildcard *.o */*.o */*/*.o)
	@printf "  $(CLR_GRN)%-8s$(CLR_RST) All build artifacts removed\n\n" "[DONE]"

# ========== Memory Layout Inspector ==========
mem_show: $(BIN_DIR)/kernel.elf
	@printf "\n$(CLR_BLD)  ApPa OS$(CLR_RST)$(CLR_DIM)  ──  Memory Layout$(CLR_RST)\n"
	@printf "$(CLR_DIM)  ─────────────────────────────────────────$(CLR_RST)\n"
	@printf "\n  $(CLR_BLD)Build Configuration$(CLR_RST)\n"
	@printf "  $(CLR_DIM)%-24s$(CLR_RST) %s\n" "KERNEL_OFFSET"   "$(KERNEL_OFFSET)"
	@printf "  $(CLR_DIM)%-24s$(CLR_RST) %s\n" "STACK_BASE"      "$(STACK_BASE)"
	@printf "  $(CLR_DIM)%-24s$(CLR_RST) %s\n" "REAL_MODE_STACK"  "$(REAL_MODE_STACK)"
	@printf "\n  $(CLR_BLD)Image Sizes$(CLR_RST)\n"
	@BOOT=$$(stat -c%s $(BIN_DIR)/boot_sector.bin 2>/dev/null || echo '?'); \
	 STG2=$$(stat -c%s $(BIN_DIR)/stage2.bin 2>/dev/null || echo '?'); \
	 KERN=$$(stat -c%s $(BIN_DIR)/kernel.bin 2>/dev/null || echo '?'); \
	 IMG=$$(stat -c%s $(BIN_DIR)/image.bin 2>/dev/null || echo '?'); \
	 KSEC=$$(( ($$KERN + 511) / 512 )); \
	 printf "  $(CLR_DIM)%-24s$(CLR_RST) %s bytes\n" "boot_sector.bin" "$$BOOT"; \
	 printf "  $(CLR_DIM)%-24s$(CLR_RST) %s bytes\n" "stage2.bin"      "$$STG2"; \
	 printf "  $(CLR_DIM)%-24s$(CLR_RST) %s bytes  (%s sectors)\n" "kernel.bin" "$$KERN" "$$KSEC"; \
	 printf "  $(CLR_DIM)%-24s$(CLR_RST) %s bytes\n" "image.bin"       "$$IMG"
	@printf "\n  $(CLR_BLD)Physical Memory Map$(CLR_RST)\n"
	@KERN_SIZE=$$(stat -c%s $(BIN_DIR)/kernel.bin 2>/dev/null || echo 0); \
	 KERN_END=$$(printf "0x%05X" $$(($(KERNEL_OFFSET) + $$KERN_SIZE))); \
	 STACK_KB=$$(( ($(STACK_BASE) - $(KERNEL_OFFSET) - $$KERN_SIZE) / 1024 )); \
	 printf "  $(CLR_DIM)0x00000000 - 0x000003FF$(CLR_RST)  IVT\n"; \
	 printf "  $(CLR_DIM)0x00000400 - 0x000007FF$(CLR_RST)  BIOS Data Area\n"; \
	 printf "  $(CLR_DIM)0x00000600 - 0x000007FF$(CLR_RST)  Stage 2 bootloader\n"; \
	 printf "  $(CLR_CYN)%-23s$(CLR_RST)  Kernel code (%s bytes)\n" "$(KERNEL_OFFSET) - $$KERN_END" "$$KERN_SIZE"; \
	 printf "  $(CLR_DIM)%-23s$(CLR_RST)  Free / Stack (~%s KB)\n" "$$KERN_END - $(STACK_BASE)" "$$STACK_KB"; \
	 printf "  $(CLR_DIM)0x000A0000 - 0x000BFFFF$(CLR_RST)  VGA Video Memory\n"; \
	 printf "  $(CLR_DIM)0x000C0000 - 0x000FFFFF$(CLR_RST)  BIOS ROM\n"; \
	 printf "  $(CLR_DIM)0x00100000 - 0x001FFFFF$(CLR_RST)  Kernel heap (kmalloc)\n"; \
	 printf "  $(CLR_DIM)0x00200000 - 0x00FFFFFF$(CLR_RST)  PMM page pool (identity mapped)\n"
	@printf "\n  $(CLR_BLD)ELF Sections$(CLR_RST)\n"
	@$(CROSS_COMPILER_BINS)objdump -h $(BIN_DIR)/kernel.elf 2>/dev/null | \
	 awk '/^[[:space:]]+[0-9]+/{printf "  %-16s  VMA: %s  Size: %s\n", $$2, $$4, $$3}'
	@printf "\n  $(CLR_BLD)Top 15 Symbols by Size$(CLR_RST)\n"
	@$(CROSS_COMPILER_BINS)nm --size-sort -r $(BIN_DIR)/kernel.elf 2>/dev/null | \
	 head -15 | awk '{ \
	   sz=strtonum("0x" $$1); \
	   sec=($$2=="T"||$$2=="t"?"code":$$2=="B"||$$2=="b"?"bss":$$2=="D"||$$2=="d"?"data":$$2); \
	   printf "  %6d B  %-24s  (%s)\n", sz, $$3, sec \
	 }'
	@if [ -f $(DISK_IMG) ]; then \
	 DSIZE=$$(stat -c%s $(DISK_IMG)); \
	 DSEC=$$(($$DSIZE / 512)); \
	 printf "\n  $(CLR_BLD)Persistent Disk$(CLR_RST)\n"; \
	 printf "  $(CLR_DIM)%-24s$(CLR_RST) %s bytes  (%s sectors)\n" "disk.img" "$$DSIZE" "$$DSEC"; \
	 MAGIC=$$(dd if=$(DISK_IMG) bs=4 count=1 2>/dev/null | od -An -tx4 | tr -d ' '); \
	 if [ "$$MAGIC" = "41504653" ]; then \
	   printf "  $(CLR_DIM)%-24s$(CLR_RST) $(CLR_GRN)SimpleFS formatted$(CLR_RST)\n" "Status"; \
	 else \
	   printf "  $(CLR_DIM)%-24s$(CLR_RST) $(CLR_YLW)Unformatted$(CLR_RST)\n" "Status"; \
	 fi; \
	else \
	 printf "\n  $(CLR_DIM)No persistent disk image found$(CLR_RST)\n"; \
	fi
	@printf "\n"

# ========== SimpleFS Image Inspector ==========
# Usage: make mem_img_show  OR  make mem_img_show IMG=path/to/disk.img
IMG ?= $(DISK_IMG)

mem_img_show:
	@python3 tools/simplefs_inspect.py $(IMG)

.PHONY: all help build run run-graphics run-term run-log debug clean check disk-reset mem_show mem_img_show _build_banner _build_done
