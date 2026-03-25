; Stage 2 Bootloader
; Loaded by boot sector at 0x0600
; Loads the full kernel from disk, switches to protected mode, and jumps to it

[bits 16]
[org 0x0600]

; Memory configuration
%ifndef KERNEL_OFFSET
	KERNEL_OFFSET equ 0x1000
%endif

%ifndef STACK_BASE
	STACK_BASE equ 0x9FC00
%endif

stage2_start:
	; Boot drive is passed in DL by boot sector
	mov [BOOT_DRIVE], dl

	; CRITICAL: Relocate stack above kernel binary load area
	; Boot sector set SP=0x9000, but kernel binary loads to 0x1000-0x90B0+
	; which overwrites the stack during INT 13h reads.
	; Move stack to SS:SP = 0x7000:0x0000 → physical 0x70000-0x7FFFE
	; This is safely above the kernel load area (~0x90B0) and below VGA (0xA0000)
	mov ax, 0x7000
	mov ss, ax
	xor sp, sp
	mov bp, sp
	
	mov si, MSG_STAGE2
	call print_rm
	call print_nl_rm

	; Load the kernel
	call load_kernel_stage2
	
	mov si, MSG_SWITCHING
	call print_rm
	call print_nl_rm
	
	; Switch to protected mode
	call switch_to_pm_stage2
	
	; Never returns
	jmp $

load_kernel_stage2:
	mov si, MSG_LOADING_KERNEL
	call print_rm
	call print_nl_rm
	
	; Load kernel to KERNEL_OFFSET
	mov word [buffer_ptr], KERNEL_OFFSET
	
	; Kernel starts at LBA sector 5 (sector 1 = boot, 2-5 = stage2)
	; We track position using LBA and convert to CHS for INT 13h
	mov byte [lba_sector], 5
KERNEL_SECTORS_STAGE2_PATCH:  ; Makefile will patch this location
	mov byte [sectors_remaining], 0  
	
.load_loop:
	mov al, [sectors_remaining]
	cmp al, 0
	je .done
	
	; Convert LBA to CHS
	; For hard disk in QEMU: 63 sectors/track, 16 heads
	; CHS: sector = (LBA % sectors_per_track) + 1
	;       head   = (LBA / sectors_per_track) % heads
	;       cyl    = (LBA / sectors_per_track) / heads
	
	mov al, [lba_sector]
	xor ah, ah
	push bx
	mov bl, 63              ; sectors per track (QEMU default for HDD)
	div bl                  ; AL = LBA/63 (track*head combo), AH = LBA%63
	mov cl, ah
	inc cl                  ; CHS sector = (LBA % 63) + 1  (1-based)
	
	; AL now = LBA / sectors_per_track
	xor ah, ah
	mov bl, 16              ; heads per cylinder (QEMU default)
	div bl                  ; AL = cylinder, AH = head
	mov ch, al              ; cylinder
	mov dh, ah              ; head
	
	; Calculate how many sectors until end of current track
	movzx ax, cl            ; current CHS sector (1-based)
	mov bl, 64              ; 63 + 1 (sector numbers are 1-63)
	sub bl, al              ; sectors left on this track
	movzx ax, bl
	pop bx                  ; restore BX (not needed yet, but clean)
	
	; Don't read more than remaining
	cmp al, [sectors_remaining]
	jle .cap_ok
	mov al, [sectors_remaining]
.cap_ok:
	; Don't read more than 127 at a time (safe limit)
	cmp al, 127
	jle .do_read
	mov al, 127
.do_read:
	; Save count for bookkeeping
	mov [read_count], al
	
	; Set up INT 13h
	mov bx, [buffer_ptr]    ; load buffer pointer into BX for INT 13h
	mov ah, 0x02            ; BIOS read function
	mov dl, [BOOT_DRIVE]
	; CH = cylinder (set above)
	; CL = sector (set above)  
	; DH = head (set above)
	; BX = buffer pointer
	
	int 0x13
	jc .disk_error
	
	; Update tracking
	mov al, [read_count]
	sub [sectors_remaining], al
	add [lba_sector], al
	
	; Advance buffer pointer (512 bytes per sector)
	movzx ax, byte [read_count]
	shl ax, 9               ; multiply by 512
	add [buffer_ptr], ax
	
	jmp .load_loop

.done:
	ret

.disk_error:
	mov si, DISK_ERROR_MSG
	call print_rm
	jmp $

; Switch to protected mode
switch_to_pm_stage2:
	cli
	lgdt [gdt_descriptor_stage2]
	mov eax, cr0
	or eax, 1
	mov cr0, eax
	jmp CODE_SEG_STAGE2:init_pm_stage2

[bits 32]
init_pm_stage2:
	mov ax, DATA_SEG_STAGE2
	mov ds, ax
	mov ss, ax
	mov es, ax
	mov fs, ax
	mov gs, ax
	
	mov ebp, STACK_BASE
	mov esp, ebp
	
	; Jump to kernel (use absolute jump via register)
	mov eax, KERNEL_OFFSET
	jmp eax
	
	; Halt if kernel returns
	jmp $

[bits 16]
; Simple print routine for real mode
print_rm:
	pusha
.loop:
	lodsb
	cmp al, 0
	je .done
	mov ah, 0x0E
	int 0x10
	jmp .loop
.done:
	popa
	ret

print_nl_rm:
	pusha
	mov ah, 0x0E
	mov al, 0x0D
	int 0x10
	mov al, 0x0A
	int 0x10
	popa
	ret

; GDT for stage2
gdt_start_stage2:
	dq 0x0

gdt_code_stage2:
	dw 0xFFFF
	dw 0x0
	db 0x0
	db 10011010b
	db 11001111b
	db 0x0

gdt_data_stage2:
	dw 0xFFFF
	dw 0x0
	db 0x0
	db 10010010b
	db 11001111b
	db 0x0

gdt_end_stage2:

gdt_descriptor_stage2:
	dw gdt_end_stage2 - gdt_start_stage2 - 1
	dd gdt_start_stage2

CODE_SEG_STAGE2 equ gdt_code_stage2 - gdt_start_stage2
DATA_SEG_STAGE2 equ gdt_data_stage2 - gdt_start_stage2

; Data
BOOT_DRIVE: db 0
lba_sector: db 0
sectors_remaining: db 0
read_count: db 0
buffer_ptr: dw 0

MSG_STAGE2: db "Stage 2 Loader", 0
MSG_LOADING_KERNEL: db "Loading kernel...", 0
MSG_SWITCHING: db "Switching to PM", 0
DISK_ERROR_MSG: db "Stage2 disk error", 0

; Pad to 2KB (4 sectors)
times 2048 - ($-$$) db 0
