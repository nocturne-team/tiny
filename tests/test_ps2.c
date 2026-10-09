#include "test.h"
#include "input/ps2_decode.h"

static int feed(const uint8_t *codes, size_t n, struct key_event *events, int max) {
    struct ps2_decoder d = { 0 };
    struct key_event ev;
    int k = 0;
    for (size_t i = 0; i < n; i++)
        if (ps2_decode(&d, codes[i], &ev) && k < max)
            events[k++] = ev;
    return k;
}

#define FEED(...) do { \
    static const uint8_t codes[] = { __VA_ARGS__ }; \
    count = feed(codes, sizeof codes, ev, 8); \
} while (0)

static size_t raw_feed(const uint8_t *codes, size_t n, struct raw_key *out, size_t max) {
    struct ps2_raw r = { 0 };
    struct raw_key tmp[2];
    size_t k = 0;
    for (size_t i = 0; i < n; i++) {
        size_t m = ps2_raw_feed(&r, codes[i], tmp);
        for (size_t j = 0; j < m; j++)
            if (k < max)
                out[k++] = tmp[j];
    }
    return k;
}

#define RAW(...) do { \
    static const uint8_t codes[] = { __VA_ARGS__ }; \
    rn = raw_feed(codes, sizeof codes, rk, 8); \
} while (0)

int main(void) {
    struct key_event ev[8];
    int count;
    struct raw_key rk[8];
    size_t rn;

    FEED(0xE0, 0x48, 0xE0, 0xC8);  // up press + release
    CHECK_EQ(count, 1);
    CHECK_EQ(ev[0].key, KEY_UP);

    FEED(0xE0, 0x4B, 0xE0, 0x4D, 0xE0, 0x50, 0xE0, 0x49, 0xE0, 0x51, 0xE0, 0x47, 0xE0, 0x4F);
    CHECK_EQ(count, 7);
    CHECK_EQ(ev[0].key, KEY_LEFT);
    CHECK_EQ(ev[1].key, KEY_RIGHT);
    CHECK_EQ(ev[2].key, KEY_DOWN);
    CHECK_EQ(ev[3].key, KEY_PGUP);
    CHECK_EQ(ev[4].key, KEY_PGDN);
    CHECK_EQ(ev[5].key, KEY_HOME);
    CHECK_EQ(ev[6].key, KEY_END);

    FEED(0x48);  // keypad 8 with NumLock off
    CHECK_EQ(count, 1);
    CHECK_EQ(ev[0].key, KEY_UP);

    FEED(0x3B, 0x3C, 0x3D, 0x01, 0x1C, 0xE0, 0x1C);
    CHECK_EQ(count, 6);
    CHECK_EQ(ev[0].key, KEY_F1);
    CHECK_EQ(ev[1].key, KEY_F2);
    CHECK_EQ(ev[2].key, KEY_F3);
    CHECK_EQ(ev[3].key, KEY_ESC);
    CHECK_EQ(ev[4].key, KEY_ENTER);
    CHECK_EQ(ev[5].key, KEY_ENTER);  // keypad enter

    FEED(0x3E, 0x3F, 0x40, 0x41, 0x42, 0x43);  // F4-F9
    CHECK_EQ(count, 6);
    CHECK_EQ(ev[0].key, KEY_F4);
    CHECK_EQ(ev[1].key, KEY_F5);
    CHECK_EQ(ev[2].key, KEY_F6);
    CHECK_EQ(ev[3].key, KEY_F7);
    CHECK_EQ(ev[4].key, KEY_F8);
    CHECK_EQ(ev[5].key, KEY_F9);

    FEED(0x13, 0x93, 0x1F, 0x02, 0x0B);  // r press, r release, s, 1, 0
    CHECK_EQ(count, 4);
    CHECK_EQ(ev[0].key, KEY_CHAR);
    CHECK_EQ(ev[0].ch, 'r');
    CHECK_EQ(ev[1].ch, 's');
    CHECK_EQ(ev[2].ch, '1');
    CHECK_EQ(ev[3].ch, '0');

    FEED(0xE0, 0x2A, 0xE0, 0x37, 0xE0, 0xB7, 0xE0, 0xAA);  // Print Screen press/release
    CHECK_EQ(count, 0);

    FEED(0xE1, 0x1D, 0x45, 0xE1, 0x9D, 0xC5);  // Pause
    CHECK_EQ(count, 0);

    FEED(0xFA, 0xFE, 0xE0, 0x5B);  // controller ACK/resend bytes, Windows key
    CHECK_EQ(count, 0);

    FEED(0x13, 0x13, 0x13);  // r held down: typematic repeats are flagged
    CHECK_EQ(count, 3);
    CHECK(!ev[0].repeat);
    CHECK(ev[1].repeat);
    CHECK(ev[2].repeat);

    FEED(0x13, 0x93, 0x13);  // r pressed twice
    CHECK_EQ(count, 2);
    CHECK(!ev[0].repeat);
    CHECK(!ev[1].repeat);

    FEED(0xE0, 0x50, 0xE0, 0x50, 0xE0, 0xD0, 0x50);  // held grey down, released; then keypad down
    CHECK_EQ(count, 3);
    CHECK(!ev[0].repeat);
    CHECK(ev[1].repeat);
    CHECK(!ev[2].repeat);  // keypad 2 is a different physical key

    FEED(0xE0, 0x2A, 0x48);  // a fake shift must not leave the E0 flag set
    CHECK_EQ(count, 1);
    CHECK_EQ(ev[0].key, KEY_UP);

    FEED(0x44);  // F10
    CHECK_EQ(count, 1);
    CHECK_EQ(ev[0].key, KEY_F10);

    // Set 1 -> usage table, a sample from every block.
    CHECK_EQ(ps2_set1_usage(0x01, false), 0x29);  // Esc
    CHECK_EQ(ps2_set1_usage(0x1E, false), 0x04);  // A
    CHECK_EQ(ps2_set1_usage(0x0B, false), 0x27);  // 0
    CHECK_EQ(ps2_set1_usage(0x1C, false), 0x28);  // Enter
    CHECK_EQ(ps2_set1_usage(0x1C, true), 0x58);   // keypad Enter
    CHECK_EQ(ps2_set1_usage(0x2A, false), 0xE1);  // left Shift
    CHECK_EQ(ps2_set1_usage(0x44, false), 0x43);  // F10
    CHECK_EQ(ps2_set1_usage(0x57, false), 0x44);  // F11
    CHECK_EQ(ps2_set1_usage(0x58, false), 0x45);  // F12
    CHECK_EQ(ps2_set1_usage(0x56, false), 0x64);  // ISO <>
    CHECK_EQ(ps2_set1_usage(0x52, false), 0x62);  // keypad 0
    CHECK_EQ(ps2_set1_usage(0x52, true), 0x49);   // Insert
    CHECK_EQ(ps2_set1_usage(0x35, true), 0x54);   // keypad /
    CHECK_EQ(ps2_set1_usage(0x38, true), 0xE6);   // right Alt
    CHECK_EQ(ps2_set1_usage(0x5B, true), 0xE3);   // left Win
    CHECK_EQ(ps2_set1_usage(0x5D, true), 0x65);   // Menu
    CHECK_EQ(ps2_set1_usage(0x20, true), 0x7F);   // Mute
    CHECK_EQ(ps2_set1_usage(0x73, false), 0x87);  // Ro
    CHECK_EQ(ps2_set1_usage(0x7D, false), 0x89);  // Yen
    CHECK_EQ(ps2_set1_usage(0x22, true), 0);      // media Play: no keyboard usage
    CHECK_EQ(ps2_set1_usage(0x5F, false), 0);
    CHECK_EQ(ps2_set1_usage(0x80, false), 0);
    // No two codes share a usage (within plain, within E0).
    for (int ext = 0; ext < 2; ext++) {
        uint8_t seen[256] = { 0 };
        for (unsigned c = 0; c < 0x80; c++) {
            uint8_t u = ps2_set1_usage((uint8_t)c, ext);
            if (u) {
                CHECK(!seen[u]);
                seen[u] = 1;
            }
        }
    }

    RAW(0x1E, 0x9E);  // A make, break
    CHECK_EQ(rn, 2);
    CHECK_EQ(rk[0].src, RAW_PS2);
    CHECK(rk[0].down);
    CHECK_EQ(rk[0].usage, 0x04);
    CHECK_EQ(rk[0].flags, 0);
    CHECK_EQ(rk[0].len, 1);
    CHECK_EQ(rk[0].bytes[0], 0x1E);
    CHECK(!rk[1].down);
    CHECK_EQ(rk[1].usage, 0x04);
    CHECK_EQ(rk[1].bytes[0], 0x9E);

    RAW(0xE0, 0x48, 0xE0, 0xC8);  // grey Up
    CHECK_EQ(rn, 2);
    CHECK_EQ(rk[0].usage, 0x52);
    CHECK_EQ(rk[0].len, 2);
    CHECK_EQ(rk[0].bytes[0], 0xE0);
    CHECK_EQ(rk[0].bytes[1], 0x48);
    CHECK(!rk[1].down);

    RAW(0xE0, 0x2A, 0xE0, 0x37, 0xE0, 0xB7, 0xE0, 0xAA);  // PrintScreen with fake shifts
    CHECK_EQ(rn, 2);
    CHECK_EQ(rk[0].usage, 0x46);
    CHECK(rk[0].down);
    CHECK_EQ(rk[1].usage, 0x46);
    CHECK(!rk[1].down);

    RAW(0xE1, 0x1D, 0x45, 0xE1, 0x9D, 0xC5, 0x1E);  // Pause, then A
    CHECK_EQ(rn, 3);
    CHECK_EQ(rk[0].usage, 0x48);
    CHECK(rk[0].down);
    CHECK_EQ(rk[0].len, 6);
    CHECK_EQ(rk[0].bytes[5], 0xC5);
    CHECK_EQ(rk[1].usage, 0x48);
    CHECK(!rk[1].down);
    CHECK_EQ(rk[2].usage, 0x04);
    CHECK_EQ(rk[2].len, 1);

    RAW(0x1D, 0x1D, 0x9D);  // left Ctrl held: typematic repeat flagged
    CHECK_EQ(rn, 3);
    CHECK_EQ(rk[0].usage, 0xE0);
    CHECK_EQ(rk[0].flags, 0);
    CHECK_EQ(rk[1].flags, RAW_REPEAT);
    CHECK(!rk[2].down);

    RAW(0xE0, 0x22, 0xE0, 0xA2);  // media Play: unmapped, bytes kept
    CHECK_EQ(rn, 2);
    CHECK_EQ(rk[0].usage, 0);
    CHECK(rk[0].flags & RAW_UNMAPPED);
    CHECK_EQ(rk[0].len, 2);
    CHECK_EQ(rk[0].bytes[1], 0x22);

    RAW(0xFA, 0xFE, 0x00, 0xFF);  // ACK, resend, overrun: not keys
    CHECK_EQ(rn, 0);

    RAW(0xE0, 0x2A, 0x1E);  // a fake shift then A: A has no E0 in its bytes
    CHECK_EQ(rn, 1);
    CHECK_EQ(rk[0].usage, 0x04);
    CHECK_EQ(rk[0].len, 1);

    // HID LED bits (0 Num, 1 Caps, 2 Scroll) -> PS/2 (0 Scroll, 1 Num, 2 Caps).
    CHECK_EQ(ps2_led_bits(0), 0);
    CHECK_EQ(ps2_led_bits(1), 2);
    CHECK_EQ(ps2_led_bits(2), 4);
    CHECK_EQ(ps2_led_bits(4), 1);
    CHECK_EQ(ps2_led_bits(7), 7);

    TEST_MAIN_END();
}
