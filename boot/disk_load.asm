; input:  dh - number of sectors, dl - drive number, write into es:bx
; output: data read into es:bx
;
; Modified to handle large sector counts by reading in smaller chunks
; This avoids BIOS limitations with single large reads (>63 sectors)

disk_load:
	pusha
	push dx              ; Save original sector count and drive

	mov byte [sectors_remaining], dh  ; Store total sectors to read
	mov byte [current_sector], 0x02    ;  Start at sector 2 (after boot sector)
	mov byte [drive_num], dl           ; Save drive number

.read_loop:
	mov al, [sectors_remaining]
	cmp al, 0
	je .done                     ; All sectors read
	
	; Read max 18 sectors at a time (safe for all BIOS)
	cmp al, 18
	jle .read_chunk
	mov al, 18                   ; Limit to 18 sectors per read
	
.read_chunk:
	push ax                       ; Save number of sectors to read
	
	mov ah, 0x02                  ; BIOS read sectors function
	mov dl, [drive_num]           ; Drive number
	mov cl, [current_sector]      ; Starting sector
	mov ch, 0x00                  ; Cylinder 0
	mov dh, 0x00                  ; Head 0
	
	int 0x13
	jc disk_error
	
	pop dx                        ; DL = number of sectors read
	mov al, dl
	
	; Update counters
	sub [sectors_remaining], al   ; Decrease remaining sectors
	add [current_sector], al      ; Advance to next sector
	
	; Advance buffer pointer (512 bytes per sector)
	movzx ax, dl
	shl ax, 9                     ; Multiply by 512 (shift left 9 bits)
	add bx, ax                    ; Advance buffer pointer
	
	jmp .read_loop

.done:
	pop dx
	popa
	ret


disk_error:
	mov bx, DISK_ERROR
	call print
	call print_nl
	mov dh, ah
	call print_hex
	jmp disk_loop

sectors_error:
	mov bx, SECTORS_ERROR
	call print

disk_loop:
	jmp $

sectors_remaining: db 0
current_sector: db 0
drive_num: db 0
DISK_ERROR: db 'Disk Read Error', 0
SECTORS_ERROR: db 'incorrect number of sectors read', 0