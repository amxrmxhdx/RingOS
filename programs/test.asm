BITS 32
org 0x101000

section .text
global _start
_start:
    mov al, 'X'
    mov ah, 0x0F
    mov edi, 0xB8000
    mov [edi], ax

    ; Exit cleanly
    mov eax, 0      ; syscall number 0 (exit)
    int 0x80        ; trigger syscall

align 4
times 512-($-$$) db 0
