#include "test.h"
#include "usb/hid_mouse.h"

int main(void) {
    struct pointer_event p;
    const uint8_t r3[3] = { 0x01, 0x05, 0xFB };
    CHECK(hid_mouse_report(r3, 3, &p));
    CHECK_EQ(p.buttons, 1);
    CHECK_EQ(p.dx, 5);
    CHECK_EQ(p.dy, -5);
    CHECK_EQ(p.wheel, 0);
    // Vendor bits above the three buttons are dropped; deltas are signed.
    const uint8_t r4[4] = { 0xFA, 0x80, 0x7F, 0xFF };
    CHECK(hid_mouse_report(r4, 4, &p));
    CHECK_EQ(p.buttons, 2);
    CHECK_EQ(p.dx, -128);
    CHECK_EQ(p.dy, 127);
    CHECK_EQ(p.wheel, -1);
    const uint8_t r8[8] = { 0, 1, 2, 1, 9, 9, 9, 9 };
    CHECK(hid_mouse_report(r8, 8, &p));
    CHECK_EQ(p.wheel, 1);
    CHECK(!hid_mouse_report(r3, 2, &p));
    CHECK(!hid_mouse_report(r3, 0, &p));
    TEST_MAIN_END();
}
