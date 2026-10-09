#pragma once
#include <stdbool.h>
#include <stdint.h>

enum key {
    KEY_NONE, KEY_UP, KEY_DOWN, KEY_LEFT, KEY_RIGHT, KEY_PGUP, KEY_PGDN,
    KEY_HOME, KEY_END, KEY_F1, KEY_F2, KEY_F3, KEY_F4, KEY_F5,
    KEY_F6, KEY_F7, KEY_F8, KEY_ENTER, KEY_ESC, KEY_CHAR,
};

struct key_event {
    enum key key;
    char ch;      // lowercase ASCII for KEY_CHAR, otherwise 0
    bool repeat;  // typematic repeat of a key that is still held down
};

struct pointer_event {
    int16_t dx, dy;   // relative motion, +x right, +y down
    int8_t wheel;     // + = away from the user (scroll up)
    uint8_t buttons;  // bit 0 left, 1 right, 2 middle
};

// Input front: merges the PS/2 and USB backends.
// Call after usb_init so the PS/2 probe sees the 8042 without firmware emulation.
void input_init(void);
// Non-blocking: returns true and fills *out when a key was pressed.
bool input_poll(struct key_event *out);
// True while a PS/2 controller responded or a USB keyboard is attached.
bool input_have_keyboard(void);
// True once each time input_have_keyboard() flips.
bool input_keyboard_changed(void);
// Non-blocking: true and fills *out for each queued pointer event. Call after input_poll.
bool input_poll_pointer(struct pointer_event *out);
