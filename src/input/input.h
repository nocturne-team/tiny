#pragma once
#include <stdbool.h>
#include <stdint.h>

enum key {
    KEY_NONE, KEY_UP, KEY_DOWN, KEY_LEFT, KEY_RIGHT, KEY_PGUP, KEY_PGDN,
    KEY_HOME, KEY_END, KEY_F1, KEY_F2, KEY_F3, KEY_F4, KEY_F5,
    KEY_F6, KEY_F7, KEY_F8, KEY_F9, KEY_F10, KEY_ENTER, KEY_ESC, KEY_CHAR,
};

struct key_event {
    enum key key;
    char ch;      // lowercase ASCII for KEY_CHAR, otherwise 0
    bool repeat;  // typematic repeat of a key that is still held down
};

// Raw key events for the key tester: every press and release, identified by HID usage.
enum raw_src { RAW_PS2, RAW_USB };
#define RAW_REPEAT 0x01    // PS/2 typematic make of a key already down
#define RAW_ROLLOVER 0x02  // USB ErrorRollOver report: too many keys / ghosting
#define RAW_UNMAPPED 0x04  // PS/2 code with no usage; see bytes[]

struct raw_key {
    uint8_t src;           // enum raw_src
    bool down;
    uint8_t usage;         // HID Keyboard/Keypad usage; 0 for RAW_ROLLOVER and RAW_UNMAPPED
    uint8_t flags;
    uint8_t len;           // PS/2: scancode bytes in bytes[]
    uint8_t bytes[6];
    char port[16];         // USB: "2" or "2.1"
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
