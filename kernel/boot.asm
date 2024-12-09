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
    resb 16384         ; 16 KiB stack
stack_top:

section .text
global _start
extern kernel_main
_start:
    mov esp, stack_top ; Set up the stack pointer
    
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

global isr80
extern isr80_handler

section .text
isr80:
    push ds
    push es
    push fs
    push gs
    pusha

    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax

    ; Now ESP points to the saved registers (the layout that matches registers_t)
    ; Pass ESP as an argument to isr80_handler
    mov eax, esp
    push eax
    call isr80_handler
    add esp, 4

    popa
    pop gs
    pop fs
    pop es
    pop ds
    iret

global gdt_flush
extern gp
gdt_flush:
    ; Load the GDT pointer into the GDTR
    lgdt [gp]

    ; Reload segment registers with new GDT values
    mov ax, 0x10        ; Data segment selector (GDT entry 2)
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax

    ; Reload code segment
    jmp 0x08:.flush     ; Jump to code segment selector (GDT entry 1)
.flush:
    ret