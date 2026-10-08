#include "input/ps2_decode.h"

static const char set1_ascii[0x3A] = {
    [0x02] = '1', [0x03] = '2', [0x04] = '3', [0x05] = '4', [0x06] = '5',
    [0x07] = '6', [0x08] = '7', [0x09] = '8', [0x0A] = '9', [0x0B] = '0',
    [0x10] = 'q', [0x11] = 'w', [0x12] = 'e', [0x13] = 'r', [0x14] = 't',
    [0x15] = 'y', [0x16] = 'u', [0x17] = 'i', [0x18] = 'o', [0x19] = 'p',
    [0x1E] = 'a', [0x1F] = 's', [0x20] = 'd', [0x21] = 'f', [0x22] = 'g',
    [0x23] = 'h', [0x24] = 'j', [0x25] = 'k', [0x26] = 'l',
    [0x2C] = 'z', [0x2D] = 'x', [0x2E] = 'c', [0x2F] = 'v', [0x30] = 'b',
    [0x31] = 'n', [0x32] = 'm', [0x39] = ' ',
};

bool ps2_decode(struct ps2_decoder *d, uint8_t sc, struct key_event *out) {
    if (sc == 0xE0) {
        d->e0 = true;
        return false;
    }
    bool ext = d->e0;
    d->e0 = false;
    if (sc & 0x80)
        return false;  // releases, plus 0xE1/0xFA/0xFE controller bytes

    // Navigation keys: grey keys send E0 xx, keypad keys (NumLock off) send xx.
    enum key k = KEY_NONE;
    switch (sc) {
    case 0x48: k = KEY_UP; break;
    case 0x50: k = KEY_DOWN; break;
    case 0x4B: k = KEY_LEFT; break;
    case 0x4D: k = KEY_RIGHT; break;
    case 0x49: k = KEY_PGUP; break;
    case 0x51: k = KEY_PGDN; break;
    case 0x47: k = KEY_HOME; break;
    case 0x4F: k = KEY_END; break;
    case 0x1C: k = KEY_ENTER; break;
    default: break;
    }

    if (k == KEY_NONE && !ext) {
        switch (sc) {
        case 0x01: k = KEY_ESC; break;
        case 0x3B: k = KEY_F1; break;
        case 0x3C: k = KEY_F2; break;
        case 0x3D: k = KEY_F3; break;
        default:
            if (sc < sizeof set1_ascii && set1_ascii[sc]) {
                out->key = KEY_CHAR;
                out->ch = set1_ascii[sc];
                return true;
            }
        }
    }

    if (k == KEY_NONE)
        return false;
    out->key = k;
    out->ch = 0;
    return true;
}
