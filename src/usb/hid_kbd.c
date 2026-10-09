#include "usb/hid_kbd.h"
#include "lib/string.h"

#define HID_ERR_ROLLOVER 0x01

static bool map_usage(uint8_t u, struct key_event *ev) {
    static const char digits[] = "1234567890";
    ev->ch = 0;
    ev->repeat = false;
    if (u >= 0x04 && u <= 0x1D) {
        ev->key = KEY_CHAR;
        ev->ch = (char)('a' + (u - 0x04));
        return true;
    }
    if (u >= 0x1E && u <= 0x27) {
        ev->key = KEY_CHAR;
        ev->ch = digits[u - 0x1E];
        return true;
    }
    switch (u) {
    case 0x28:
    case 0x58: ev->key = KEY_ENTER; return true;  // Enter, keypad Enter
    case 0x29: ev->key = KEY_ESC; return true;
    case 0x2C: ev->key = KEY_CHAR; ev->ch = ' '; return true;
    case 0x3A: ev->key = KEY_F1; return true;
    case 0x3B: ev->key = KEY_F2; return true;
    case 0x3C: ev->key = KEY_F3; return true;
    case 0x3D: ev->key = KEY_F4; return true;
    case 0x3E: ev->key = KEY_F5; return true;
    case 0x3F: ev->key = KEY_F6; return true;
    case 0x40: ev->key = KEY_F7; return true;
    case 0x41: ev->key = KEY_F8; return true;
    case 0x42: ev->key = KEY_F9; return true;
    case 0x43: ev->key = KEY_F10; return true;
    case 0x44: ev->key = KEY_F11; return true;
    case 0x4A: ev->key = KEY_HOME; return true;
    case 0x4B: ev->key = KEY_PGUP; return true;
    case 0x4D: ev->key = KEY_END; return true;
    case 0x4E: ev->key = KEY_PGDN; return true;
    case 0x4F: ev->key = KEY_RIGHT; return true;
    case 0x50: ev->key = KEY_LEFT; return true;
    case 0x51: ev->key = KEY_DOWN; return true;
    case 0x52: ev->key = KEY_UP; return true;
    default: return false;
    }
}

static bool has(const uint8_t *keys, size_t n, uint8_t u) {
    for (size_t i = 0; i < n; i++)
        if (keys[i] == u)
            return true;
    return false;
}

size_t hid_kbd_report(struct hid_kbd *k, const uint8_t *r, size_t len, uint64_t now_ms,
                      struct key_event *out) {
    if (len < 3)
        return 0;
    uint8_t cur[8] = { 0 };
    memcpy(cur, r, len < 8 ? len : 8);
    if (has(cur + 2, 6, HID_ERR_ROLLOVER))
        return 0;  // too many keys down: keep the last good state

    size_t n = 0;
    for (size_t i = 2; i < 8; i++) {
        uint8_t u = cur[i];
        if (u < 0x04 || has(k->prev + 2, 6, u) || has(cur + 2, i - 2, u))
            continue;  // empty, error code, still held, or a duplicate in this report
        if (map_usage(u, &out[n])) {
            n++;
            k->held = u;
            k->next_repeat_ms = now_ms + HID_REPEAT_DELAY_MS;
        }
    }
    if (k->held && !has(cur + 2, 6, k->held))
        k->held = 0;
    memcpy(k->prev, cur, sizeof cur);
    return n;
}

bool hid_kbd_tick(struct hid_kbd *k, uint64_t now_ms, struct key_event *out) {
    if (!k->held || now_ms < k->next_repeat_ms)
        return false;
    k->next_repeat_ms = now_ms + HID_REPEAT_RATE_MS;
    map_usage(k->held, out);
    out->repeat = true;
    return true;
}

void hid_kbd_cancel_repeat(struct hid_kbd *k) {
    k->held = 0;
}

static void raw_event(struct raw_key *o, bool down, uint8_t usage) {
    memset(o, 0, sizeof *o);
    o->src = RAW_USB;
    o->down = down;
    o->usage = usage;
}

size_t hid_kbd_diff(struct hid_kbd *k, const uint8_t *r, size_t len, struct raw_key *out) {
    if (len < 3)
        return 0;
    uint8_t cur[8] = { 0 };
    memcpy(cur, r, len < 8 ? len : 8);
    if (has(cur + 2, 6, HID_ERR_ROLLOVER)) {
        if (k->raw_rollover)
            return 0;
        k->raw_rollover = true;
        raw_event(out, true, 0);
        out->flags = RAW_ROLLOVER;
        return 1;
    }
    k->raw_rollover = false;
    const uint8_t *prev = k->raw_prev;
    size_t n = 0;
    for (unsigned b = 0; b < 8; b++)
        if ((cur[0] & ~prev[0]) & (1u << b))
            raw_event(&out[n++], true, (uint8_t)(0xE0 + b));
    for (size_t i = 2; i < 8; i++) {
        uint8_t u = cur[i];
        if (u >= 0x04 && !has(prev + 2, 6, u) && !has(cur + 2, i - 2, u))
            raw_event(&out[n++], true, u);
    }
    for (unsigned b = 0; b < 8; b++)
        if ((prev[0] & ~cur[0]) & (1u << b))
            raw_event(&out[n++], false, (uint8_t)(0xE0 + b));
    for (size_t i = 2; i < 8; i++) {
        uint8_t u = prev[i];
        if (u >= 0x04 && !has(cur + 2, 6, u) && !has(prev + 2, i - 2, u))
            raw_event(&out[n++], false, u);
    }
    memcpy(k->raw_prev, cur, sizeof cur);
    return n;
}
