#ifndef RINGOS_KEYBOARD_H
#define RINGOS_KEYBOARD_H

#include "types.h"

// Keyboard ports
#define KEYBOARD_DATA_PORT    0x60
#define KEYBOARD_STATUS_PORT  0x64
#define KEYBOARD_COMMAND_PORT 0x64

// Keyboard status
#define KEYBOARD_OUTPUT_FULL  0x01
#define KEYBOARD_INPUT_FULL   0x02

// Special keys
#define KEY_ESC    0x01
#define KEY_BSPACE 0x0E
#define KEY_TAB    0x0F
#define KEY_ENTER  0x1C
#define KEY_LCTRL  0x1D
#define KEY_LSHIFT 0x2A
#define KEY_RSHIFT 0x36
#define KEY_LALT   0x38
#define KEY_CAPS   0x3A

// Function prototypes
void keyboard_init(void);
char keyboard_read(void);
bool keyboard_is_shift_pressed(void);
bool keyboard_is_caps_on(void);

#endif /* RINGOS_KEYBOARD_H */