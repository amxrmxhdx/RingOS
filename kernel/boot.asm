; Multiboot header constants
MBOOT_HEADER_MAGIC  equ 0x1BADB002
MBOOT_PAGE_ALIGN    equ 1<<0
MBOOT_MEM_INFO      equ 1<<1
MBOOT_HEADER_FLAGS  equ MBOOT_PAGE_ALIGN | MBOOT_MEM_INFO
MBOOT_CHECKSUM      equ -(MBOOT_HEADER_MAGIC + MBOOT_HEADER_FLAGS)

section .multiboot
align 4
    dd MBOOT_HEADER_MAGIC
    dd MBOOT_HEADER_FLAGS
    dd MBOOT_CHECKSUM

section .bss
align 16
stack_bottom:
    resb 16384 ; 16 KiB
stack_top:

section .text
global _start
extern kernel_main

_start:
    ; Initialize stack
    mov esp, stack_top
    
    ; Reset EFLAGS
    push 0
    popf
    
    ; Clear screen and reset attributes
    mov edi, 0xB8000
    mov ax, 0x0F20   ; Black background (0), white foreground (F), space character (20)
    mov ecx, 80*25   ; Full screen
    rep stosw        ; Fill screen with default attributes
    
    ; Reset cursor position
    mov dx, 0x3D4
    mov al, 0x0F
    out dx, al
    inc dx
    xor al, al
    out dx, al
    dec dx
    mov al, 0x0E
    out dx, al
    inc dx
    xor al, al
    out dx, al
    
    ; Call kernel
    call kernel_main

    ; Halt if kernel returns
    cli
.hang:
    hlt
    jmp .hang