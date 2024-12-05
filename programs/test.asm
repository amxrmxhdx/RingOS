BITS 32
ORG 0x100000

start:
    ; Write a single 'X' character at the top-left corner
    mov byte [0xB8000], 'X'    ; Character
    mov byte [0xB8001], 0x0F   ; White on black
    ret                        ; Return to kernel