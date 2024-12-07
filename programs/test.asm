; test.asm
BITS 32
org 0x101000

start:
    ; Do some work
    mov eax, 0x12345
    
    ; Exit via syscall
    mov eax, 0    ; SYSCALL_EXIT
    int 0x80      ; Trigger syscall

times 512 db 0