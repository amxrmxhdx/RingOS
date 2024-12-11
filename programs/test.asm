[BITS 32]
[ORG 0x101000]

section .data
    ; Buffer for text content
    text_buffer: times 2000 db 0
    buffer_size equ 2000

    ; Screen dimensions
    COLS equ 80
    ROWS equ 25

    ; Editor state
    cursor_x dd 0
    cursor_y dd 0
    mode db 0        ; 0 = normal mode, 1 = insert mode
    last_scancode db 0
    last_ascii db 0

    ; Messages
    normal_msg db "-- NORMAL --", 0
    insert_msg db "-- INSERT --", 0
    debug_msg db "Scancode: ", 0
    ascii_msg db " ASCII: ", 0
    mode_msg db " Mode: ", 0

    ; Scancode to ASCII lookup table
    scancode_table:
        db 0   ; 0x00 - None
        db 27  ; 0x01 - Escape
        db '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '='  ; 0x02 - 0x0D
        db 8   ; 0x0E - Backspace
        db 9   ; 0x0F - Tab
        db 'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']'  ; 0x10 - 0x1B
        db 13  ; 0x1C - Enter
        db 0   ; 0x1D - Left Control
        db 'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', "'", '`'  ; 0x1E - 0x29
        db 0   ; 0x2A - Left Shift
        db '\' ; 0x2B - Backslash
        db 'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/'            ; 0x2C - 0x35
        db 0   ; 0x36 - Right Shift
        db '*' ; 0x37 - Keypad *
        db 0   ; 0x38 - Left Alt
        db ' ' ; 0x39 - Space

section .text
global _start

_start:
    call clear_screen
    call init_editor

main_loop:
    call display_status
    call update_cursor

    ; Get key and filter break codes
    call wait_for_key
    mov [last_scancode], al

    ; Convert scancode to ASCII and store
    call scancode_to_ascii
    mov [last_ascii], al

    ; Show debug info
    call show_debug_info

    ; Check for break code
    test byte [last_scancode], 0x80
    jnz main_loop    ; Skip break codes

    ; Process based on mode
    cmp byte [mode], 0
    je .normal_mode
    jmp .insert_mode

.normal_mode:
    call handle_normal_mode
    jmp main_loop

.insert_mode:
    call handle_insert_mode
    jmp main_loop

show_debug_info:
    push eax
    push ebx
    push ecx

    ; Position at top of screen
    mov ebx, 0xB8000

    ; Show scancode
    mov esi, debug_msg
    call print_string

    movzx eax, byte [last_scancode]
    call print_hex_byte

    ; Show ASCII
    mov esi, ascii_msg
    call print_string

    movzx eax, byte [last_ascii]
    call print_hex_byte

    ; Show mode
    mov esi, mode_msg
    call print_string

    movzx eax, byte [mode]
    add al, '0'
    mov ah, 0x07
    mov [ebx], ax

    pop ecx
    pop ebx
    pop eax
    ret

print_hex_byte:
    push eax
    push ebx

    mov bl, al

    ; High nibble
    shr al, 4
    call print_hex_digit

    ; Low nibble
    mov al, bl
    and al, 0x0F
    call print_hex_digit

    pop ebx
    pop eax
    ret

print_hex_digit:
    and al, 0x0F
    add al, '0'
    cmp al, '9'
    jle .store
    add al, 7
.store:
    mov ah, 0x07
    mov [ebx], ax
    add ebx, 2
    ret

wait_for_key:
    push ecx
    push edx

.wait:
    in al, 0x64
    test al, 1
    jz .wait

    in al, 0x60

    pop edx
    pop ecx
    ret

scancode_to_ascii:
    push ebx

    ; Clear break code bit and check range
    movzx ebx, al
    and ebx, 0x7F
    cmp ebx, 0x40        ; Check if within table bounds
    jae .no_convert

    ; Convert using table
    mov al, [scancode_table + ebx]

    pop ebx
    ret

.no_convert:
    xor al, al
    pop ebx
    ret

handle_normal_mode:
    ; Check for command keys
    cmp byte [last_scancode], 0x17  ; 'i' scancode
    je enter_insert_mode
    cmp byte [last_scancode], 0x23  ; 'h' scancode
    je move_left
    cmp byte [last_scancode], 0x26  ; 'l' scancode
    je move_right
    cmp byte [last_scancode], 0x24  ; 'j' scancode
    je move_down
    cmp byte [last_scancode], 0x25  ; 'k' scancode
    je move_up
    cmp byte [last_scancode], 0x10  ; 'q' scancode
    je exit_editor
    ret

handle_insert_mode:
    push eax

    ; Get the ASCII value we stored
    mov al, [last_ascii]

    ; Check for ESC key
    cmp byte [last_scancode], 0x01
    je .escape_pressed

    ; Check if it's a printable character
    cmp al, ' '
    jl .done
    cmp al, '~'
    jg .done

    ; Display the character
    push eax
    call insert_char
    pop eax

.done:
    pop eax
    ret

.escape_pressed:
    pop eax
    jmp enter_normal_mode

enter_insert_mode:
    mov byte [mode], 1
    ret

enter_normal_mode:
    mov byte [mode], 0
    ret

insert_char:
    push eax
    push ebx
    push edx

    ; Calculate screen position
    mov eax, [cursor_y]
    mov ebx, COLS * 2
    mul ebx
    mov ebx, [cursor_x]
    shl ebx, 1
    add eax, ebx
    add eax, 0xB8000

    ; Display character with attribute
    mov bl, [last_ascii]
    mov byte [eax], bl      ; Character
    mov byte [eax + 1], 0x07  ; Attribute

    ; Move cursor right
    call move_right

    pop edx
    pop ebx
    pop eax
    ret

move_left:
    cmp dword [cursor_x], 0
    je .done
    dec dword [cursor_x]
.done:
    ret

move_right:
    cmp dword [cursor_x], COLS-1
    je .done
    inc dword [cursor_x]
.done:
    ret

move_up:
    cmp dword [cursor_y], 0
    je .done
    dec dword [cursor_y]
.done:
    ret

move_down:
    cmp dword [cursor_y], ROWS-2
    je .done
    inc dword [cursor_y]
.done:
    ret

clear_screen:
    push eax
    push ecx
    push edi

    mov edi, 0xB8000
    mov ecx, COLS * ROWS
    mov ax, 0x0720      ; Black background, white text, space
    rep stosw

    pop edi
    pop ecx
    pop eax
    ret

update_cursor:
    push eax
    push ebx
    push edx

    ; Calculate cursor position
    mov eax, [cursor_y]
    mov ebx, COLS
    mul ebx
    add eax, [cursor_x]

    ; Update hardware cursor low byte
    mov dx, 0x3D4
    mov al, 0x0F
    out dx, al
    inc dx
    mov al, bl
    out dx, al

    ; Update hardware cursor high byte
    dec dx
    mov al, 0x0E
    out dx, al
    inc dx
    mov al, bh
    out dx, al

    pop edx
    pop ebx
    pop eax
    ret

display_status:
    push eax
    push ebx
    push esi

    ; Position at bottom of screen
    mov ebx, 0xB8000 + ((ROWS-1) * COLS * 2)

    ; Clear status line
    push ecx
    mov ecx, COLS
    mov ax, 0x0720
    rep stosw
    pop ecx

    ; Select message based on mode
    mov esi, normal_msg
    cmp byte [mode], 0
    je .show_status
    mov esi, insert_msg

.show_status:
    call print_string

    pop esi
    pop ebx
    pop eax
    ret

print_string:
    push eax
    push ebx

.loop:
    lodsb
    test al, al
    jz .done
    mov ah, 0x07    ; Normal attribute
    mov [ebx], ax
    add ebx, 2
    jmp .loop

.done:
    pop ebx
    pop eax
    ret

init_editor:
    mov dword [cursor_x], 0
    mov dword [cursor_y], 0
    mov byte [mode], 0
    ret

exit_editor:
    ret
