[bits 16]
switch_to_pm:
	cli
	lgdt [gdt_descriptor]
	mov eax, cr0
	or eax, 0x1
	mov cr0, eax
	jmp CODE_SEG:init_pm

[bits 32]
init_pm:
	mov ax, DATA_SEG
	mov ds, ax
	mov ss, ax
	mov es, ax
	mov fs, ax
	mov gs, ax

	; Stack configuration (can be overridden by makefile with -D STACK_BASE=0x...)
	%ifndef STACK_BASE
		STACK_BASE equ 0x9FC00  ; Default: 639KB (607KB stack space)
	%endif
	
	mov ebp, STACK_BASE
	mov esp, ebp

	call BEGIN_PM