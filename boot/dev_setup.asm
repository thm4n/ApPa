; methode to setup VGA text mode : 10h mode 03

set_VGA_text_mode:
	pusha

	push dx

	mov ah, 0x02
	mov al, dh
	mov cl, 0x02

	mov ch, 0x00
	mov dh, 0x00

	int 0x10
	jc disk_error

	pop dx
	cmp al,dh
	jne sectors_error
	popa
	ret
