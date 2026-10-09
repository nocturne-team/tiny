#include "ui/kbd_layout.h"

#define K(u, l, x, w, r) { u, l, x, w, r, 1 }
#define T(u, l, x, w, r) { u, l, x, w, r, 2 }  // two rows tall

// x and w in quarter units. Main block 0-60, nav 61-73, keypad 74-90.
const struct kbd_key kbd_keys[KBD_KEYS] = {
    // Row 0
    K(0x29, "Esc", 0, 4, 0),
    K(0x3A, "F1", 8, 4, 0), K(0x3B, "F2", 12, 4, 0), K(0x3C, "F3", 16, 4, 0), K(0x3D, "F4", 20, 4, 0),
    K(0x3E, "F5", 26, 4, 0), K(0x3F, "F6", 30, 4, 0), K(0x40, "F7", 34, 4, 0), K(0x41, "F8", 38, 4, 0),
    K(0x42, "F9", 44, 4, 0), K(0x43, "F10", 48, 4, 0), K(0x44, "F11", 52, 4, 0), K(0x45, "F12", 56, 4, 0),
    K(0x46, "PrtSc", 61, 4, 0), K(0x47, "ScrLk", 65, 4, 0), K(0x48, "Pause", 69, 4, 0),
    // Row 1
    K(0x35, "`", 0, 4, 1), K(0x1E, "1", 4, 4, 1), K(0x1F, "2", 8, 4, 1), K(0x20, "3", 12, 4, 1),
    K(0x21, "4", 16, 4, 1), K(0x22, "5", 20, 4, 1), K(0x23, "6", 24, 4, 1), K(0x24, "7", 28, 4, 1),
    K(0x25, "8", 32, 4, 1), K(0x26, "9", 36, 4, 1), K(0x27, "0", 40, 4, 1), K(0x2D, "-", 44, 4, 1),
    K(0x2E, "=", 48, 4, 1), K(0x2A, "Bksp", 52, 8, 1),
    K(0x49, "Ins", 61, 4, 1), K(0x4A, "Home", 65, 4, 1), K(0x4B, "PgUp", 69, 4, 1),
    K(0x53, "Num", 74, 4, 1), K(0x54, "/", 78, 4, 1), K(0x55, "*", 82, 4, 1), K(0x56, "-", 86, 4, 1),
    // Row 2
    K(0x2B, "Tab", 0, 6, 2), K(0x14, "Q", 6, 4, 2), K(0x1A, "W", 10, 4, 2), K(0x08, "E", 14, 4, 2),
    K(0x15, "R", 18, 4, 2), K(0x17, "T", 22, 4, 2), K(0x1C, "Y", 26, 4, 2), K(0x18, "U", 30, 4, 2),
    K(0x0C, "I", 34, 4, 2), K(0x12, "O", 38, 4, 2), K(0x13, "P", 42, 4, 2), K(0x2F, "[", 46, 4, 2),
    K(0x30, "]", 50, 4, 2), K(0x31, "\\", 54, 6, 2),
    K(0x4C, "Del", 61, 4, 2), K(0x4D, "End", 65, 4, 2), K(0x4E, "PgDn", 69, 4, 2),
    K(0x5F, "7", 74, 4, 2), K(0x60, "8", 78, 4, 2), K(0x61, "9", 82, 4, 2), T(0x57, "+", 86, 4, 2),
    // Row 3
    K(0x39, "Caps", 0, 7, 3), K(0x04, "A", 7, 4, 3), K(0x16, "S", 11, 4, 3), K(0x07, "D", 15, 4, 3),
    K(0x09, "F", 19, 4, 3), K(0x0A, "G", 23, 4, 3), K(0x0B, "H", 27, 4, 3), K(0x0D, "J", 31, 4, 3),
    K(0x0E, "K", 35, 4, 3), K(0x0F, "L", 39, 4, 3), K(0x33, ";", 43, 4, 3), K(0x34, "'", 47, 4, 3),
    K(0x28, "Enter", 51, 9, 3),
    K(0x5C, "4", 74, 4, 3), K(0x5D, "5", 78, 4, 3), K(0x5E, "6", 82, 4, 3),
    // Row 4
    K(0xE1, "Shift", 0, 5, 4), K(0x64, "<>", 5, 4, 4), K(0x1D, "Z", 9, 4, 4), K(0x1B, "X", 13, 4, 4),
    K(0x06, "C", 17, 4, 4), K(0x19, "V", 21, 4, 4), K(0x05, "B", 25, 4, 4), K(0x11, "N", 29, 4, 4),
    K(0x10, "M", 33, 4, 4), K(0x36, ",", 37, 4, 4), K(0x37, ".", 41, 4, 4), K(0x38, "/", 45, 4, 4),
    K(0xE5, "Shift", 49, 11, 4),
    K(0x52, "\x18", 65, 4, 4),
    K(0x59, "1", 74, 4, 4), K(0x5A, "2", 78, 4, 4), K(0x5B, "3", 82, 4, 4), T(0x58, "Enter", 86, 4, 4),
    // Row 5
    K(0xE0, "Ctrl", 0, 5, 5), K(0xE3, "Win", 5, 5, 5), K(0xE2, "Alt", 10, 5, 5),
    K(0x2C, "Space", 15, 25, 5), K(0xE6, "Alt", 40, 5, 5), K(0xE7, "Win", 45, 5, 5),
    K(0x65, "Menu", 50, 5, 5), K(0xE4, "Ctrl", 55, 5, 5),
    K(0x50, "\x1b", 61, 4, 5), K(0x51, "\x19", 65, 4, 5), K(0x4F, "\x1a", 69, 4, 5),
    K(0x62, "0", 74, 8, 5), K(0x63, ".", 82, 4, 5),
};

bool kbd_place(int cols, int rows, struct kbd_geom *g) {
    int unit = cols * 4 / KBD_WIDTH_Q;  // floor(cols / 22.5)
    if (unit > 6)
        unit = 6;
    if (unit < 3)
        return false;
    g->unit = unit;
    g->key_h = unit >= 5 && rows >= 32 ? 3 : 1;
    int pitch = g->key_h == 3 ? 3 : 2;  // rows per key row (one-row keys: a blank row between)
    int gap = g->key_h == 3 ? 1 : 0;    // extra row below the F-key row
    g->width = KBD_WIDTH_Q * unit / 4;
    g->height = 6 * pitch + gap - (pitch - g->key_h);
    if (g->height > rows)
        return false;
    for (int i = 0; i < KBD_KEYS; i++) {
        const struct kbd_key *k = &kbd_keys[i];
        int x0 = k->x * unit / 4, x1 = (k->x + k->w) * unit / 4;
        g->r[i] = (struct kbd_rect){
            .x = x0,
            .y = k->row * pitch + (k->row ? gap : 0),
            .w = x1 - x0,
            .h = (k->h - 1) * pitch + g->key_h,
        };
    }
    return true;
}

static const char *const names[256] = {
    [0x04] = "A", [0x05] = "B", [0x06] = "C", [0x07] = "D", [0x08] = "E", [0x09] = "F",
    [0x0A] = "G", [0x0B] = "H", [0x0C] = "I", [0x0D] = "J", [0x0E] = "K", [0x0F] = "L",
    [0x10] = "M", [0x11] = "N", [0x12] = "O", [0x13] = "P", [0x14] = "Q", [0x15] = "R",
    [0x16] = "S", [0x17] = "T", [0x18] = "U", [0x19] = "V", [0x1A] = "W", [0x1B] = "X",
    [0x1C] = "Y", [0x1D] = "Z",
    [0x1E] = "1", [0x1F] = "2", [0x20] = "3", [0x21] = "4", [0x22] = "5", [0x23] = "6",
    [0x24] = "7", [0x25] = "8", [0x26] = "9", [0x27] = "0",
    [0x28] = "Enter", [0x29] = "Esc", [0x2A] = "Backspace", [0x2B] = "Tab", [0x2C] = "Space",
    [0x2D] = "-", [0x2E] = "=", [0x2F] = "[", [0x30] = "]", [0x31] = "\\", [0x32] = "ISO #~",
    [0x33] = ";", [0x34] = "'", [0x35] = "`", [0x36] = ",", [0x37] = ".", [0x38] = "/",
    [0x39] = "Caps Lock",
    [0x3A] = "F1", [0x3B] = "F2", [0x3C] = "F3", [0x3D] = "F4", [0x3E] = "F5", [0x3F] = "F6",
    [0x40] = "F7", [0x41] = "F8", [0x42] = "F9", [0x43] = "F10", [0x44] = "F11", [0x45] = "F12",
    [0x46] = "PrintScreen", [0x47] = "Scroll Lock", [0x48] = "Pause", [0x49] = "Insert",
    [0x4A] = "Home", [0x4B] = "Page Up", [0x4C] = "Delete", [0x4D] = "End", [0x4E] = "Page Down",
    [0x4F] = "Right", [0x50] = "Left", [0x51] = "Down", [0x52] = "Up",
    [0x53] = "Num Lock", [0x54] = "Keypad /", [0x55] = "Keypad *", [0x56] = "Keypad -",
    [0x57] = "Keypad +", [0x58] = "Keypad Enter", [0x59] = "Keypad 1", [0x5A] = "Keypad 2",
    [0x5B] = "Keypad 3", [0x5C] = "Keypad 4", [0x5D] = "Keypad 5", [0x5E] = "Keypad 6",
    [0x5F] = "Keypad 7", [0x60] = "Keypad 8", [0x61] = "Keypad 9", [0x62] = "Keypad 0",
    [0x63] = "Keypad .", [0x64] = "ISO <>", [0x65] = "Menu", [0x66] = "Power",
    [0x7F] = "Mute", [0x80] = "Volume Up", [0x81] = "Volume Down",
    [0x87] = "Ro", [0x88] = "Katakana", [0x89] = "Yen", [0x8A] = "Henkan", [0x8B] = "Muhenkan",
    [0x90] = "Hangul", [0x91] = "Hanja",
    [0xE0] = "Left Ctrl", [0xE1] = "Left Shift", [0xE2] = "Left Alt", [0xE3] = "Left Win",
    [0xE4] = "Right Ctrl", [0xE5] = "Right Shift", [0xE6] = "Right Alt", [0xE7] = "Right Win",
};

const char *kbd_usage_name(uint8_t usage) {
    return names[usage];
}
