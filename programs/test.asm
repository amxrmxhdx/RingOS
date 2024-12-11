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
    last_char db 0   ; Store the actual character
    shift_pressed db 0 ; Track shift key state

    ; Messages
    welcome_msg db "VIM Editor - Press i for insert mode, ESC for normal mode, q to quit", 0
    normal_msg db "-- NORMAL MODE --", 0
    insert_msg db "-- INSERT MODE --", 0
    debug_msg db "Scancode: ", 0

    ; Key definitions
    KEY_ESC     equ 0x01
    KEY_ENTER   equ 0x1C
    KEY_LSHIFT  equ 0x2A
    KEY_RSHIFT  equ 0x36
    KEY_DELETE  equ 0x53
    KEY_BACKSPACE equ 0x0E
    KEY_SPACE   equ 0x39

    ; Scancode to ASCII lookup tables
    scancode_table:
        ;     0    1    2    3    4    5    6    7    8    9    A    B    C    D    E    F
        db    0,   0,  '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=',  08,  09  ; 0
        db   'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']',  13,   0, 'a', 's'  ; 1
        db   'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', "'", '`',   0, '\', 'z', 'x', 'c', 'v'  ; 2
        db   'b', 'n', 'm', ',', '.', '/',   0, '*',   0, ' ',   0,   0,   0,   0,   0,   0  ; 3
        times 128-64 db 0  ; Pad the rest

    scancode_table_shift:
        ;     0    1    2    3    4    5    6    7    8    9    A    B    C    D    E    F
        db    0,   0,  '!', '@', '#', '$', '%', '^', '&', '*', '(', ')', '_', '+',  08,  09  ; 0
        db   'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', '{', '}',  13,   0, 'A', 'S'  ; 1
        db   'D', 'F', 'G', 'H', 'J', 'K', 'L', ':', '"', '~',   0, '|', 'Z', 'X', 'C', 'V'  ; 2
        db   'B', 'N', 'M', '<', '>', '?',   0, '*',   0, ' ',   0,   0,   0,   0,   0,   0  ; 3
        times 128-64 db 0  ; Pad the rest

section .text
global _start

_start:
    call clear_screen
    call init_editor

    ; Show welcome message
    mov ebx, 0xB8000 + (COLS * 2)  ; Second row
    mov esi, welcome_msg
    call print_string

main_loop:
    call display_status
    call update_cursor

    ; Get keypress
    call wait_for_key
    mov [last_scancode], al

    ; Skip break codes (except for shift)
    test al, 0x80
    jnz .check_shift_release

    ; Convert scancode to character
    call scancode_to_char
    mov [last_char], al

    ; Show debug info
    push eax
    mov ebx, 0xB8000
    mov esi, debug_msg
    call print_string
    movzx eax, byte [last_scancode]
    call print_hex
    pop eax

    ; Process based on mode
    cmp byte [mode], 0
    je .normal_mode
    jmp .insert_mode

.check_shift_release:
    mov bl, al
    and bl, 0x7F    ; Clear break bit
    cmp bl, KEY_LSHIFT
    je .handle_shift_release
    cmp bl, KEY_RSHIFT
    je .handle_shift_release
    jmp main_loop

.handle_shift_release:
    mov byte [shift_pressed], 0
    jmp main_loop

.normal_mode:
    call handle_normal_mode
    jmp main_loop

.insert_mode:
    call handle_insert_mode
    jmp main_loop

scancode_to_char:
    push ebx

    ; Get the scancode (mask off the break code bit)
    movzx ebx, byte [last_scancode]
    mov al, bl
    and al, 0x7F    ; Clear break code bit

    ; Check for shift keys
    cmp al, KEY_LSHIFT
    je .handle_shift
    cmp al, KEY_RSHIFT
    je .handle_shift

    ; Handle space key specially
    cmp al, KEY_SPACE
    je .handle_space

    ; Check if it's in range for character conversion
    cmp al, 64  ; Maximum scancode we handle
    jae .no_char

    ; Get character based on shift state
    movzx ebx, al   ; Clear upper bits, keep scancode
    mov al, [scancode_table + ebx]  ; Get normal character
    test byte [shift_pressed], 1
    jz .done
    mov al, [scancode_table_shift + ebx]  ; Get shifted character if shift pressed
    jmp .done

.handle_shift:
    mov byte [shift_pressed], 1
    jmp .no_char

.handle_space:
    mov al, ' '
    jmp .done

.no_char:
    xor al, al

.done:
    pop ebx
    ret

handle_normal_mode:
    movzx eax, byte [last_scancode]

    cmp al, 0x10      ; 'q'
    je exit_editor
    cmp al, 0x17      ; 'i'
    je enter_insert_mode
    cmp al, 0x23      ; 'h'
    je move_left
    cmp al, 0x26      ; 'l'
    je move_right
    cmp al, 0x24      ; 'j'
    je move_down
    cmp al, 0x25      ; 'k'
    je move_up
    ret

handle_insert_mode:
    ; Check for ESC key
    cmp byte [last_scancode], KEY_ESC
    je enter_normal_mode

    ; Check for Enter key
    cmp byte [last_scancode], KEY_ENTER
    je handle_enter

    ; Check for backspace
    cmp byte [last_scancode], KEY_BACKSPACE
    je handle_backspace

    ; Get the character we stored
    mov al, [last_char]
    test al, al
    jz .done

    ; Display the character
    call display_char

.done:
    ret

display_char:
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

    ; Write character and attribute
    mov bl, [last_char]    ; Get the actual character to display
    mov bh, 0x07           ; Light gray on black
    mov [eax], bx

    ; Move cursor right
    call move_right

    pop edx
    pop ebx
    pop eax
    ret

handle_enter:
    mov dword [cursor_x], 0
    inc dword [cursor_y]
    cmp dword [cursor_y], ROWS-1
    jl .done
    dec dword [cursor_y]
.done:
    ret

handle_backspace:
    ; Don't backspace at start of line
    cmp dword [cursor_x], 0
    je .done

    ; Move cursor left
    call move_left

    ; Clear character at current position
    push eax
    push ebx

    ; Calculate screen position
    mov eax, [cursor_y]
    mov ebx, COLS * 2
    mul ebx
    mov ebx, [cursor_x]
    shl ebx, 1
    add eax, ebx
    add eax, 0xB8000

    ; Write space with attribute
    mov word [eax], 0x0720    ; Space with normal attribute

    pop ebx
    pop eax

.done:
    ret

enter_insert_mode:
    mov byte [mode], 1
    ret

enter_normal_mode:
    mov byte [mode], 0
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

update_cursor:
    push eax
    push ebx
    push edx

    ; Calculate cursor position
    mov eax, [cursor_y]
    mov ebx, COLS
    mul ebx
    add eax, [cursor_x]

    ; Update cursor low byte
    mov dx, 0x3D4
    mov al, 0x0F
    out dx, al
    inc dx
    mov al, bl
    out dx, al

    ; Update cursor high byte
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

    ; Reset to start of status line
    mov ebx, 0xB8000 + ((ROWS-1) * COLS * 2)

    ; Select message based on mode
    mov esi, normal_msg
    cmp byte [mode], 0
    je .show
    mov esi, insert_msg
.show:
    call print_string

    pop esi
    pop ebx
    pop eax
    ret

print_string:
    push eax
.loop:
    lodsb
    test al, al
    jz .done
    mov ah, 0x07
    mov [ebx], ax
    add ebx, 2
    jmp .loop
.done:
    pop eax
    ret

print_hex:
    push eax
    push ebx

    mov bl, al
    shr al, 4
    call print_hex_digit
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
.wait:
    in al, 0x64
    test al, 1
    jz .wait
    in al, 0x60
    ret

init_editor:
    mov dword [cursor_x], 0
    mov dword [cursor_y], 0
    mov byte [mode], 0
    mov byte [shift_pressed], 0
    ret

exit_editor:
    call clear_screen
    mov dword [cursor_x], 0
    mov dword [cursor_y], 0
    call update_cursor
    xor eax, eax
    ret
