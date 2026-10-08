#pragma once
#include <stdbool.h>

enum key {
    KEY_NONE, KEY_UP, KEY_DOWN, KEY_LEFT, KEY_RIGHT, KEY_PGUP, KEY_PGDN,
    KEY_HOME, KEY_END, KEY_F1, KEY_F2, KEY_F3, KEY_ENTER, KEY_ESC, KEY_CHAR,
};

struct key_event {
    enum key key;
    char ch;  // lowercase ASCII for KEY_CHAR, otherwise 0
};

// Keyboard backend (PS/2 today; a USB backend can implement the same pair).
// input_init returns false when no keyboard controller responds.
bool input_init(void);
// Non-blocking: returns true and fills *out when a key was pressed.
bool input_poll(struct key_event *out);
