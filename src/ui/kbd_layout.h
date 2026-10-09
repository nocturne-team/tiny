#pragma once
#include <stdbool.h>
#include <stdint.h>

// The key tester's picture: full-size ANSI (104 keys) plus the ISO <> key.
struct kbd_key {
    uint8_t usage;      // HID Keyboard/Keypad usage
    const char *label;  // short; cut to fit
    uint8_t x, w;       // quarter units from the left edge
    uint8_t row, h;     // row 0 = Esc and F-keys, 1-5 below; h = rows spanned (1 or 2)
};

#define KBD_KEYS 105
#define KBD_WIDTH_Q 90  // 22.5 units: main block, nav cluster, keypad, 1/4-unit gaps

extern const struct kbd_key kbd_keys[KBD_KEYS];

struct kbd_rect {
    int x, y, w, h;     // cells, relative to the board's top left corner
};

struct kbd_geom {
    int unit;           // columns per unit (3-6)
    int key_h;          // 3: boxed keys; 1: one row, the background shows the key
    int width, height;  // the board, in cells
    struct kbd_rect r[KBD_KEYS];
};

// Fits the board into a cols x rows content area. False when it can't (draw text only).
bool kbd_place(int cols, int rows, struct kbd_geom *g);
// "A", "Left Shift", "ISO #~", ... for any usage worth naming; NULL otherwise.
const char *kbd_usage_name(uint8_t usage);
