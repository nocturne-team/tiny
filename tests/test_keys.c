#include "test.h"
#include "ui/kbd_layout.h"
#include "ui/keys_state.h"

static struct keys_state s;
static char buf[128];

static struct raw_key usb(uint8_t usage, bool down) {
    struct raw_key e = { .src = RAW_USB, .down = down, .usage = usage };
    strcpy(e.port, "2.1");
    return e;
}

static struct raw_key ps2(uint8_t usage, bool down, uint8_t flags, uint8_t b0, uint8_t b1) {
    struct raw_key e = { .src = RAW_PS2, .down = down, .usage = usage, .flags = flags };
    e.bytes[e.len++] = b0;
    if (b1)
        e.bytes[e.len++] = b1;
    return e;
}

static bool apply(struct raw_key e, uint64_t now) {
    return keys_apply(&s, &e, now);
}

int main(void) {
    // Held, most at once, tested.
    memset(&s, 0, sizeof s);
    CHECK_EQ(keys_untested(&s), KBD_KEYS);
    apply(usb(0x04, true), 0);
    apply(usb(0x05, true), 0);
    CHECK_EQ(s.held_n, 2);
    CHECK_EQ(s.max_held, 2);
    CHECK(keys_is_held(&s, 0x04));
    apply(usb(0x04, false), 0);
    apply(usb(0x04, false), 0);  // a release of a key that isn't held changes nothing
    CHECK_EQ(s.held_n, 1);
    CHECK_EQ(s.max_held, 2);
    CHECK(!keys_is_held(&s, 0x04));
    CHECK(keys_is_tested(&s, 0x04));
    CHECK_EQ(s.tested_n, 2);
    CHECK_EQ(keys_untested(&s), KBD_KEYS - 2);
    apply(usb(0x32, true), 0);  // ISO #~ is not on the picture
    CHECK_EQ(s.tested_n, 3);
    CHECK_EQ(keys_untested(&s), KBD_KEYS - 2);

    // Event lines.
    memset(&s, 0, sizeof s);
    apply(usb(0x04, true), 0);
    keys_line_text(&s.lines[0], buf, sizeof buf);
    CHECK_STR(buf, "USB 2.1                  press    0x04  A");
    apply(usb(0x04, false), 0);
    keys_line_text(&s.lines[0], buf, sizeof buf);
    CHECK_STR(buf, "USB 2.1                  release  0x04  A");
    CHECK_EQ(s.nlines, 2);

    // PS/2 typematic repeats fold into one line.
    memset(&s, 0, sizeof s);
    apply(ps2(0x52, true, 0, 0xE0, 0x48), 0);
    for (int i = 0; i < 11; i++)
        apply(ps2(0x52, true, RAW_REPEAT, 0xE0, 0x48), 0);
    CHECK_EQ(s.nlines, 1);
    CHECK_EQ(s.held_n, 1);
    keys_line_text(&s.lines[0], buf, sizeof buf);
    CHECK_STR(buf, "PS/2 E0 48               make     0x52  Up  x12");

    // Unmapped and rollover lines; neither changes the held keys.
    memset(&s, 0, sizeof s);
    apply(ps2(0, true, RAW_UNMAPPED, 0xE0, 0x22), 0);
    keys_line_text(&s.lines[0], buf, sizeof buf);
    CHECK_STR(buf, "PS/2 E0 22               make     unmapped");
    {
        struct raw_key e = { .src = RAW_USB, .down = true, .flags = RAW_ROLLOVER };
        strcpy(e.port, "3");
        CHECK(!keys_apply(&s, &e, 0));
    }
    keys_line_text(&s.lines[0], buf, sizeof buf);
    CHECK_STR(buf, "USB 3                    rollover error (too many keys)");
    CHECK_EQ(s.held_n, 0);
    CHECK_EQ(s.tested_n, 0);

    // Pause comes as a press and its release: it isn't left held.
    memset(&s, 0, sizeof s);
    {
        struct raw_key e = { .src = RAW_PS2, .down = true, .usage = 0x48, .len = 6,
                             .bytes = { 0xE1, 0x1D, 0x45, 0xE1, 0x9D, 0xC5 } };
        keys_apply(&s, &e, 0);
        keys_line_text(&s.lines[0], buf, sizeof buf);
        CHECK_STR(buf, "PS/2 E1 1D 45 E1 9D C5   make     0x48  Pause");
        e.down = false;
        keys_apply(&s, &e, 0);
    }
    CHECK_EQ(s.held_n, 0);
    CHECK(keys_is_tested(&s, 0x48));

    // Only the newest KEYS_LINES lines are kept, newest first.
    memset(&s, 0, sizeof s);
    for (int i = 0; i < 15; i++)
        apply(usb((uint8_t)(0x04 + i), true), 0);
    CHECK_EQ(s.nlines, KEYS_LINES);
    CHECK_EQ(s.lines[0].ev.usage, 0x04 + 14);

    // Hold Esc to leave: 1000 ms, PS/2 repeats don't restart the timer.
    memset(&s, 0, sizeof s);
    apply(ps2(0x29, true, 0, 0x01, 0), 100);
    CHECK(!keys_esc_held(&s, 1099));
    apply(ps2(0x29, true, RAW_REPEAT, 0x01, 0), 600);
    CHECK(keys_esc_held(&s, 1100));
    // A tap doesn't leave.
    memset(&s, 0, sizeof s);
    apply(usb(0x29, true), 0);
    apply(usb(0x29, false), 900);
    CHECK(!keys_esc_held(&s, 5000));
    // After leaving, held keys are forgotten (their releases go to another tab), so
    // coming back doesn't leave again at once; tested keys stay.
    memset(&s, 0, sizeof s);
    apply(usb(0x29, true), 0);
    apply(usb(0x04, true), 0);
    CHECK(keys_esc_held(&s, 1000));
    keys_reset_held(&s);
    CHECK(!keys_esc_held(&s, 99999));
    CHECK_EQ(s.held_n, 0);
    CHECK(!keys_is_held(&s, 0x04));
    CHECK(keys_is_tested(&s, 0x29));
    apply(usb(0x04, true), 0);  // pressing it again counts as held once
    CHECK_EQ(s.held_n, 1);

    // Lock LEDs (HID bits: 0 Num, 1 Caps, 2 Scroll).
    memset(&s, 0, sizeof s);
    CHECK(apply(usb(0x39, true), 0));
    CHECK_EQ(s.leds, 2);
    CHECK(!apply(usb(0x39, false), 0));
    CHECK(apply(ps2(0x39, true, 0, 0x3A, 0), 0));  // released, so a new press: flips back
    CHECK_EQ(s.leds, 0);
    CHECK(!apply(ps2(0x39, true, RAW_REPEAT, 0x3A, 0), 0));  // held: repeats don't flip
    CHECK_EQ(s.leds, 0);
    CHECK(apply(usb(0x53, true), 0));
    CHECK(apply(usb(0x47, true), 0));
    CHECK_EQ(s.leds, 5);
    TEST_MAIN_END();
}
