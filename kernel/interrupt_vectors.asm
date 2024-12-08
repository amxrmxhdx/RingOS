section .text
global isr0
global isr1
; ... declare other ISRs as needed

extern isr_handler    ; This is the C function we'll call

; Macro to create interrupt handlers without error codes
%macro ISR_NOERRCODE 1
global isr%1
isr%1:
    push dword 0     ; Push dummy error code
    push dword %1    ; Push interrupt number
    jmp isr_common_stub
%endmacro

; Macro for interrupts that do push error codes (like page faults)
%macro ISR_ERRCODE 1
global isr%1
isr%1:
    push dword %1    ; Push interrupt number
    jmp isr_common_stub
%endmacro

; Create handlers for interrupts 0-31
ISR_NOERRCODE 0
ISR_NOERRCODE 1
; ... create other handlers

; Special handler for syscalls (int 0x80)
global isr80
isr80:
    push dword 0     ; Push dummy error code
    push dword 0x80  ; Push interrupt number
    jmp isr_common_stub

isr_common_stub:
    pusha           ; Pushes edi,esi,ebp,esp,ebx,edx,ecx,eax

    mov ax, ds      ; Save data segment
    push eax

    mov ax, 0x10    ; Load kernel data segment
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax

    call isr_handler

    pop eax         ; Restore data segment
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax

    popa           ; Pops edi,esi,ebp,esp,ebx,edx,ecx,eax
    add esp, 8     ; Clean up error code and interrupt number
    iret           ; Return from interrupt