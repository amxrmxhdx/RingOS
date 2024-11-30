#include "../include/keyboard.h"
#include "../include/io.h"

static bool shift_pressed = false;
static bool caps_lock = false;

// Scancode to ASCII mapping tables
static const char scancode_to_ascii[] = {
    0, 0, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', '\b',
    '\t', 'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n',
    0, 'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`',
    0, '\\', 'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/', 0,
    '*', 0, ' '
};

static const char scancode_to_ascii_shift[] = {
    0, 0, '!', '@', '#', '$', '%', '^', '&', '*', '(', ')', '_', '+', '\b',
    '\t', 'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', '{', '}', '\n',
    0, 'A', 'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L', ':', '"', '~',
    0, '|', 'Z', 'X', 'C', 'V', 'B', 'N', 'M', '<', '>', '?', 0,
    '*', 0, ' '
};

void keyboard_init(void) {
    // Wait for keyboard controller to be ready
    while (inb(KEYBOARD_STATUS_PORT) & KEYBOARD_INPUT_FULL);
    
    // Reset shift and caps lock state
    shift_pressed = false;
    caps_lock = false;
}

static char handle_scancode(uint8_t scancode) {
    // Key release (break codes)
    if (scancode & 0x80) {
        scancode &= 0x7F;  // Clear the top bit
        if (scancode == KEY_LSHIFT || scancode == KEY_RSHIFT) {
            shift_pressed = false;
        }
        return 0;
    }

    // Handle special keys
    switch (scancode) {
        case KEY_LSHIFT:
        case KEY_RSHIFT:
            shift_pressed = true;
            return 0;
        case KEY_CAPS:
            caps_lock = !caps_lock;
            return 0;
    }

    // Convert scancode to ASCII
    if (scancode >= sizeof(scancode_to_ascii)) {
        return 0;
    }

    char c;
    if (shift_pressed) {
        c = scancode_to_ascii_shift[scancode];
    } else {
        c = scancode_to_ascii[scancode];
    }

    // Handle caps lock for letters
    if (caps_lock && c >= 'a' && c <= 'z') {
        return c - 'a' + 'A';
    } else if (caps_lock && c >= 'A' && c <= 'Z') {
        return c - 'A' + 'a';
    }

    return c;
}

char keyboard_read(void) {
    char c;
    do {
        // Wait for keyboard data
        while (!(inb(KEYBOARD_STATUS_PORT) & KEYBOARD_OUTPUT_FULL));
        
        // Read scancode and convert to ASCII
        uint8_t scancode = inb(KEYBOARD_DATA_PORT);
        c = handle_scancode(scancode);
    } while (c == 0);  // Keep reading until we get a printable character

    return c;
}

bool keyboard_is_shift_pressed(void) {
    return shift_pressed;
}

bool keyboard_is_caps_on(void) {
    return caps_lock;
}