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

int main(void) {
    struct key_event ev[8];
    int count;

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

    FEED(0x3E, 0x3F);  // F4, F5
    CHECK_EQ(count, 2);
    CHECK_EQ(ev[0].key, KEY_F4);
    CHECK_EQ(ev[1].key, KEY_F5);

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

    FEED(0xE0, 0x2A, 0x48);  // a fake shift must not leave the E0 flag set
    CHECK_EQ(count, 1);
    CHECK_EQ(ev[0].key, KEY_UP);

    TEST_MAIN_END();
}
