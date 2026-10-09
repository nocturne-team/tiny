#include "input/ps2_decode.h"
#include "lib/string.h"

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
    uint8_t code = sc & 0x7F, bit = (uint8_t)(1u << (code & 7));
    uint8_t *held = &d->down[ext][code >> 3];
    if (sc & 0x80) {  // releases, plus 0xE1/0xFA/0xFE controller bytes
        *held &= (uint8_t)~bit;
        return false;
    }
    bool repeat = *held & bit;
    *held |= bit;
    out->repeat = repeat;

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
        case 0x3E: k = KEY_F4; break;
        case 0x3F: k = KEY_F5; break;
        case 0x40: k = KEY_F6; break;
        case 0x41: k = KEY_F7; break;
        case 0x42: k = KEY_F8; break;
        case 0x43: k = KEY_F9; break;
        case 0x44: k = KEY_F10; break;
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

// Set 1 (as translated by the 8042) to HID usages, after the HID Usage Tables'
// Keyboard/Keypad page.
static const uint8_t set1_plain[0x80] = {
    [0x01] = 0x29, [0x02] = 0x1E, [0x03] = 0x1F, [0x04] = 0x20, [0x05] = 0x21,
    [0x06] = 0x22, [0x07] = 0x23, [0x08] = 0x24, [0x09] = 0x25, [0x0A] = 0x26,
    [0x0B] = 0x27, [0x0C] = 0x2D, [0x0D] = 0x2E, [0x0E] = 0x2A, [0x0F] = 0x2B,
    [0x10] = 0x14, [0x11] = 0x1A, [0x12] = 0x08, [0x13] = 0x15, [0x14] = 0x17,
    [0x15] = 0x1C, [0x16] = 0x18, [0x17] = 0x0C, [0x18] = 0x12, [0x19] = 0x13,
    [0x1A] = 0x2F, [0x1B] = 0x30, [0x1C] = 0x28, [0x1D] = 0xE0, [0x1E] = 0x04,
    [0x1F] = 0x16, [0x20] = 0x07, [0x21] = 0x09, [0x22] = 0x0A, [0x23] = 0x0B,
    [0x24] = 0x0D, [0x25] = 0x0E, [0x26] = 0x0F, [0x27] = 0x33, [0x28] = 0x34,
    [0x29] = 0x35, [0x2A] = 0xE1, [0x2B] = 0x31, [0x2C] = 0x1D, [0x2D] = 0x1B,
    [0x2E] = 0x06, [0x2F] = 0x19, [0x30] = 0x05, [0x31] = 0x11, [0x32] = 0x10,
    [0x33] = 0x36, [0x34] = 0x37, [0x35] = 0x38, [0x36] = 0xE5, [0x37] = 0x55,
    [0x38] = 0xE2, [0x39] = 0x2C, [0x3A] = 0x39, [0x3B] = 0x3A, [0x3C] = 0x3B,
    [0x3D] = 0x3C, [0x3E] = 0x3D, [0x3F] = 0x3E, [0x40] = 0x3F, [0x41] = 0x40,
    [0x42] = 0x41, [0x43] = 0x42, [0x44] = 0x43, [0x45] = 0x53, [0x46] = 0x47,
    [0x47] = 0x5F, [0x48] = 0x60, [0x49] = 0x61, [0x4A] = 0x56, [0x4B] = 0x5C,
    [0x4C] = 0x5D, [0x4D] = 0x5E, [0x4E] = 0x57, [0x4F] = 0x59, [0x50] = 0x5A,
    [0x51] = 0x5B, [0x52] = 0x62, [0x53] = 0x63, [0x54] = 0x46, [0x56] = 0x64,
    [0x57] = 0x44, [0x58] = 0x45, [0x70] = 0x88, [0x73] = 0x87, [0x79] = 0x8A,
    [0x7B] = 0x8B, [0x7D] = 0x89,
};

static uint8_t set1_ext(uint8_t code) {
    switch (code) {
    case 0x1C: return 0x58;  // keypad Enter
    case 0x1D: return 0xE4;  // right Ctrl
    case 0x20: return 0x7F;  // Mute
    case 0x2E: return 0x81;  // Volume Down
    case 0x30: return 0x80;  // Volume Up
    case 0x35: return 0x54;  // keypad /
    case 0x37: return 0x46;  // PrintScreen
    case 0x38: return 0xE6;  // right Alt
    case 0x46: return 0x48;  // Ctrl+Pause (Break)
    case 0x47: return 0x4A;  // Home
    case 0x48: return 0x52;  // Up
    case 0x49: return 0x4B;  // Page Up
    case 0x4B: return 0x50;  // Left
    case 0x4D: return 0x4F;  // Right
    case 0x4F: return 0x4D;  // End
    case 0x50: return 0x51;  // Down
    case 0x51: return 0x4E;  // Page Down
    case 0x52: return 0x49;  // Insert
    case 0x53: return 0x4C;  // Delete
    case 0x5B: return 0xE3;  // left Win
    case 0x5C: return 0xE7;  // right Win
    case 0x5D: return 0x65;  // Menu
    default: return 0;
    }
}

uint8_t ps2_set1_usage(uint8_t code, bool ext) {
    if (code >= 0x80)
        return 0;
    return ext ? set1_ext(code) : set1_plain[code];
}

uint8_t ps2_led_bits(uint8_t hid) {
    return (uint8_t)((hid & 1) << 1 | (hid & 2) << 1 | (hid & 4) >> 2);
}

static const uint8_t pause_seq[6] = { 0xE1, 0x1D, 0x45, 0xE1, 0x9D, 0xC5 };

static void raw_fill(struct raw_key *o, const struct ps2_raw *r, bool down, uint8_t usage,
                     uint8_t flags) {
    memset(o, 0, sizeof *o);
    o->src = RAW_PS2;
    o->down = down;
    o->usage = usage;
    o->flags = (uint8_t)(flags | (usage ? 0 : RAW_UNMAPPED));
    o->len = r->len;
    memcpy(o->bytes, r->seq, r->len);
}

size_t ps2_raw_feed(struct ps2_raw *r, uint8_t sc, struct raw_key out[2]) {
    if (!r->len && (sc == 0xFA || sc == 0xFE || sc == 0x00 || sc == 0xFF))
        return 0;  // ACK, resend, buffer overrun / key detection error: not keys
    if ((sc == 0xE0 || sc == 0xE1) && r->len && r->seq[0] != 0xE1)
        r->len = 0;  // a new prefix inside an E0 sequence: start over
    if (r->len == sizeof r->seq)
        r->len = 0;
    r->seq[r->len++] = sc;

    if (r->seq[0] == 0xE1) {  // Pause: six bytes on press, nothing on release
        if (r->len < sizeof pause_seq)
            return 0;
        size_t n = 1;
        if (memcmp(r->seq, pause_seq, sizeof pause_seq) == 0) {
            raw_fill(&out[0], r, true, 0x48, 0);
            raw_fill(&out[1], r, false, 0x48, 0);
            n = 2;
        } else {
            raw_fill(&out[0], r, true, 0, 0);
        }
        r->len = 0;
        return n;
    }
    if (r->seq[0] == 0xE0 && r->len == 1)
        return 0;

    bool ext = r->seq[0] == 0xE0, brk = sc & 0x80;
    uint8_t code = sc & 0x7F;
    if (ext && (code == 0x2A || code == 0x36)) {  // fake shifts around PrintScreen and friends
        r->len = 0;
        return 0;
    }
    uint8_t bit = (uint8_t)(1u << (code & 7)), *held = &r->down[ext][code >> 3], flags = 0;
    if (brk) {
        *held &= (uint8_t)~bit;
    } else {
        if (*held & bit)
            flags = RAW_REPEAT;
        *held |= bit;
    }
    raw_fill(out, r, !brk, ps2_set1_usage(code, ext), flags);
    r->len = 0;
    return 1;
}
