#include "test.h"
#include "usb/hid_kbd.h"

static struct hid_kbd k;
static struct key_event ev[6];

// Feeds an 8-byte boot report holding up to three key usages.
static size_t keys(uint64_t now, uint8_t a, uint8_t b, uint8_t c) {
    const uint8_t r[8] = { 0, 0, a, b, c, 0, 0, 0 };
    return hid_kbd_report(&k, r, sizeof r, now, ev);
}

static void reset(void) {
    memset(&k, 0, sizeof k);
}

int main(void) {
    struct key_event e;

    // Press, hold, release, press again.
    reset();
    CHECK_EQ(keys(0, 0x52, 0, 0), 1);
    CHECK_EQ(ev[0].key, KEY_UP);
    CHECK_EQ(ev[0].repeat, 0);
    // Keyboards that ignore SET_IDLE resend the same report: never a new press.
    size_t dup = 0;
    for (int i = 0; i < 100; i++)
        dup += keys(8 + (uint64_t)i, 0x52, 0, 0);
    CHECK_EQ(dup, 0);
    CHECK_EQ(keys(200, 0, 0, 0), 0);
    CHECK_EQ(keys(208, 0x52, 0, 0), 1);

    // Several keys in one report come out in report order.
    reset();
    CHECK_EQ(keys(0, 0x3A, 0x3E, 0x4F), 3);
    CHECK_EQ(ev[0].key, KEY_F1);
    CHECK_EQ(ev[1].key, KEY_F5);
    CHECK_EQ(ev[2].key, KEY_RIGHT);

    // A second key while the first is held: only the new one.
    reset();
    keys(0, 0x04, 0, 0);
    CHECK_EQ(keys(10, 0x04, 0x05, 0), 1);
    CHECK_EQ(ev[0].ch, 'b');

    // Usage table.
    static const struct { uint8_t usage; enum key key; char ch; } map[] = {
        { 0x04, KEY_CHAR, 'a' }, { 0x15, KEY_CHAR, 'r' }, { 0x16, KEY_CHAR, 's' },
        { 0x1D, KEY_CHAR, 'z' }, { 0x1E, KEY_CHAR, '1' }, { 0x27, KEY_CHAR, '0' },
        { 0x2C, KEY_CHAR, ' ' }, { 0x28, KEY_ENTER, 0 }, { 0x58, KEY_ENTER, 0 },
        { 0x29, KEY_ESC, 0 }, { 0x3A, KEY_F1, 0 }, { 0x3B, KEY_F2, 0 }, { 0x3C, KEY_F3, 0 },
        { 0x3D, KEY_F4, 0 }, { 0x3E, KEY_F5, 0 }, { 0x4A, KEY_HOME, 0 }, { 0x4B, KEY_PGUP, 0 },
        { 0x4D, KEY_END, 0 }, { 0x4E, KEY_PGDN, 0 }, { 0x4F, KEY_RIGHT, 0 },
        { 0x50, KEY_LEFT, 0 }, { 0x51, KEY_DOWN, 0 }, { 0x52, KEY_UP, 0 },
    };
    for (size_t i = 0; i < sizeof map / sizeof map[0]; i++) {
        reset();
        CHECK_EQ(keys(0, map[i].usage, 0, 0), 1);
        CHECK_EQ(ev[0].key, map[i].key);
        CHECK_EQ(ev[0].ch, map[i].ch);
    }

    // Unmapped usages (F6, Caps Lock) and error codes 2-3 give nothing and never repeat.
    reset();
    CHECK_EQ(keys(0, 0x3F, 0x39, 0x02), 0);
    CHECK(!hid_kbd_tick(&k, 10000, &e));

    // The modifier byte is ignored: Shift+A is still 'a'.
    reset();
    {
        const uint8_t r[8] = { 0x02, 0, 0x04, 0, 0, 0, 0, 0 };
        CHECK_EQ(hid_kbd_report(&k, r, 8, 0, ev), 1);
        CHECK_EQ(ev[0].ch, 'a');
    }

    // ErrorRollOver is ignored and the previous state is kept.
    reset();
    CHECK_EQ(keys(0, 0x04, 0, 0), 1);
    {
        const uint8_t r[8] = { 0, 0, 1, 1, 1, 1, 1, 1 };
        CHECK_EQ(hid_kbd_report(&k, r, 8, 5, ev), 0);
    }
    CHECK_EQ(keys(10, 0x04, 0x05, 0), 1);
    CHECK_EQ(ev[0].ch, 'b');

    // Short reports are ignored, 3 bytes is enough, bytes past 8 are ignored.
    reset();
    {
        const uint8_t r2[2] = { 0, 0 };
        CHECK_EQ(hid_kbd_report(&k, r2, 2, 0, ev), 0);
        const uint8_t r3[3] = { 0, 0, 0x52 };
        CHECK_EQ(hid_kbd_report(&k, r3, 3, 0, ev), 1);
        CHECK_EQ(ev[0].key, KEY_UP);
    }
    reset();
    {
        uint8_t r16[16] = { 0, 0, 0x51 };
        memset(r16 + 8, 0x04, 8);
        CHECK_EQ(hid_kbd_report(&k, r16, 16, 0, ev), 1);
        CHECK_EQ(ev[0].key, KEY_DOWN);
    }

    // The same usage twice in one report is one press.
    reset();
    CHECK_EQ(keys(0, 0x51, 0x51, 0), 1);

    // Typematic: 500 ms delay, then every 33 ms, flagged as repeats.
    reset();
    CHECK(!hid_kbd_tick(&k, 0, &e));
    keys(1000, 0x51, 0, 0);
    CHECK(!hid_kbd_tick(&k, 1499, &e));
    CHECK(hid_kbd_tick(&k, 1500, &e));
    CHECK_EQ(e.key, KEY_DOWN);
    CHECK_EQ(e.repeat, 1);
    CHECK(!hid_kbd_tick(&k, 1532, &e));
    CHECK(hid_kbd_tick(&k, 1533, &e));
    CHECK(hid_kbd_tick(&k, 5000, &e));  // a late tick repeats once, no burst
    CHECK(!hid_kbd_tick(&k, 5001, &e));
    keys(5002, 0, 0, 0);
    CHECK(!hid_kbd_tick(&k, 9000, &e));  // released

    // A newer key takes over the repeat and restarts the delay.
    reset();
    keys(0, 0x51, 0, 0);
    keys(400, 0x51, 0x4E, 0);
    CHECK(!hid_kbd_tick(&k, 899, &e));
    CHECK(hid_kbd_tick(&k, 900, &e));
    CHECK_EQ(e.key, KEY_PGDN);
    // Releasing the older key keeps the newer one repeating.
    keys(950, 0x4E, 0, 0);
    CHECK(hid_kbd_tick(&k, 1000, &e));
    CHECK_EQ(e.key, KEY_PGDN);

    // Releasing the repeating key while an older one stays held stops repeating.
    reset();
    keys(0, 0x51, 0, 0);
    keys(10, 0x51, 0x4E, 0);
    CHECK_EQ(keys(20, 0x51, 0, 0), 0);
    CHECK(!hid_kbd_tick(&k, 10000, &e));

    // A transfer error may have eaten the release report: repeat stops, presses still work.
    reset();
    keys(0, 0x51, 0, 0);
    hid_kbd_cancel_repeat(&k);
    CHECK(!hid_kbd_tick(&k, 10000, &e));
    CHECK_EQ(keys(10, 0x51, 0x4E, 0), 1);
    CHECK_EQ(ev[0].key, KEY_PGDN);
    TEST_MAIN_END();
}
