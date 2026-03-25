[org 0x7c00]

; Memory configuration (can be overridden by makefile with -D flags)
%ifndef KERNEL_OFFSET
	KERNEL_OFFSET equ 0x1000
%endif

%ifndef REAL_MODE_STACK
	REAL_MODE_STACK equ 0x9000
%endif

	mov [BOOT_DRIVE], dl
	mov bp, REAL_MODE_STACK
	mov sp, bp

	mov bx, MSG_REAL_MODE
	call print
	call print_nl

	call load_stage2
	
	; Jump to stage2 which will load kernel and switch to PM
	jmp 0x0000:0x0600

%include "boot/print.asm"
%include "boot/print_hex.asm"
%include "boot/disk_load.asm"

[bits 16]
load_stage2:
	mov bx, MSG_LOAD_STAGE2
	call print
	call print_nl

	; Load stage 2 bootloader (4 sectors at 0x0600)
	; Placed below kernel (0x1000) so kernel load can't overwrite it
	mov bx, 0x0600           ; Load stage2 at 0x0600 (0x0600-0x0DFF = 2KB)
	mov dh, 4                 ; Load 4 sectors (2KB for stage2)
	mov dl, [BOOT_DRIVE]
	call disk_load
	
	; DL already contains boot drive, stage2 will read it
	
	ret

BOOT_DRIVE: db 0
MSG_REAL_MODE: db "STARTED IN REAL MODE", 0
MSG_LOAD_STAGE2: db "LOADING STAGE 2", 0

times 510 - ($-$$) db 0
dw 0xaa55
