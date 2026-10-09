#include "test.h"
#include "usb/usb_desc.h"

#define HID_DESC 9, 0x21, 0x11, 0x01, 0, 1, 0x22, 65, 0

// A plain boot keyboard: config (34 bytes), interface 0 (3/1/1), HID, EP 0x81.
static const uint8_t kbd_cfg[] = {
    9, 2, 34, 0, 1, 1, 0, 0xA0, 50,
    9, 4, 0, 0, 1, 3, 1, 1, 0,
    HID_DESC,
    7, 5, 0x81, 3, 8, 0, 10,
};

// A receiver: mouse on interface 0, keyboard on interface 1, plus an alternate
// setting of interface 1 whose endpoint must be ignored.
static const uint8_t combo_cfg[] = {
    9, 2, 75, 0, 2, 1, 0, 0xA0, 50,
    9, 4, 0, 0, 1, 3, 1, 2, 0, HID_DESC, 7, 5, 0x81, 3, 4, 0, 10,
    9, 4, 1, 0, 1, 3, 1, 1, 0, HID_DESC, 7, 5, 0x82, 3, 8, 0, 8,
    9, 4, 1, 1, 1, 3, 1, 1, 0, 7, 5, 0x83, 3, 16, 0, 1,
};

// Keyboard whose interface lists an interrupt OUT endpoint before the IN one.
static const uint8_t out_first_cfg[] = {
    9, 2, 32, 0, 1, 1, 0, 0xA0, 50,
    9, 4, 0, 0, 2, 3, 1, 1, 0,
    7, 5, 0x01, 3, 8, 0, 10,
    7, 5, 0x82, 3, 8, 0, 4,
};

static const uint8_t hub_cfg[] = {
    9, 2, 25, 0, 1, 1, 0, 0xE0, 0,
    9, 4, 0, 0, 1, 9, 0, 0, 0,
    7, 5, 0x81, 3, 1, 0, 12,
};

// A mouse-only device.
static const uint8_t mouse_cfg[] = {
    9, 2, 34, 0, 1, 1, 0, 0xA0, 50,
    9, 4, 0, 0, 1, 3, 1, 2, 0,
    HID_DESC,
    7, 5, 0x81, 3, 4, 0, 10,
};
int main(void) {
    struct usb_config_info ci;

    CHECK(usb_desc_config(kbd_cfg, sizeof kbd_cfg, &ci));
    CHECK_EQ(ci.value, 1);
    CHECK(ci.has_kbd);
    CHECK_EQ(ci.kbd_iface, 0);
    CHECK_EQ(ci.kbd_ep.addr, 0x81);
    CHECK_EQ(ci.kbd_ep.mps, 8);
    CHECK_EQ(ci.kbd_ep.interval, 10);
    CHECK(!ci.has_hub);

    CHECK(usb_desc_config(combo_cfg, sizeof combo_cfg, &ci));
    CHECK(ci.has_kbd);
    CHECK_EQ(ci.kbd_iface, 1);
    CHECK_EQ(ci.kbd_ep.addr, 0x82);
    CHECK_EQ(ci.kbd_ep.interval, 8);
    CHECK(ci.has_mouse);  // the receiver's mouse is on interface 0
    CHECK_EQ(ci.mouse_iface, 0);
    CHECK_EQ(ci.mouse_ep.addr, 0x81);
    CHECK_EQ(ci.mouse_ep.mps, 4);

    CHECK(usb_desc_config(mouse_cfg, sizeof mouse_cfg, &ci));
    CHECK(ci.has_mouse);
    CHECK(!ci.has_kbd);
    CHECK_EQ(ci.mouse_ep.interval, 10);
    CHECK(usb_desc_config(kbd_cfg, sizeof kbd_cfg, &ci));
    CHECK(!ci.has_mouse);

    CHECK(usb_desc_config(out_first_cfg, sizeof out_first_cfg, &ci));
    CHECK(ci.has_kbd);
    CHECK_EQ(ci.kbd_ep.addr, 0x82);

    CHECK(usb_desc_config(hub_cfg, sizeof hub_cfg, &ci));
    CHECK(!ci.has_kbd);
    CHECK(ci.has_hub);
    CHECK_EQ(ci.hub_ep.addr, 0x81);
    CHECK_EQ(ci.hub_ep.mps, 1);
    CHECK_EQ(ci.hub_ep.interval, 12);

    // Truncated in the middle of the endpoint: valid header, no keyboard.
    CHECK(usb_desc_config(kbd_cfg, 30, &ci));
    CHECK(!ci.has_kbd);

    // Non-boot HID (subclass 0) is not a keyboard for us.
    {
        uint8_t b[sizeof kbd_cfg];
        memcpy(b, kbd_cfg, sizeof b);
        b[9 + 6] = 0;
        CHECK(usb_desc_config(b, sizeof b, &ci));
        CHECK(!ci.has_kbd);
    }
    // Zero max packet size, or a bulk endpoint: ignored.
    {
        uint8_t b[sizeof kbd_cfg];
        memcpy(b, kbd_cfg, sizeof b);
        b[27 + 4] = 0;
        CHECK(usb_desc_config(b, sizeof b, &ci));
        CHECK(!ci.has_kbd);
        memcpy(b, kbd_cfg, sizeof b);
        b[27 + 3] = 2;
        CHECK(usb_desc_config(b, sizeof b, &ci));
        CHECK(!ci.has_kbd);
    }
    // A zero-length descriptor in the middle stops the walk (no endless loop).
    {
        uint8_t b[sizeof kbd_cfg];
        memcpy(b, kbd_cfg, sizeof b);
        b[18] = 0;
        CHECK(usb_desc_config(b, sizeof b, &ci));
        CHECK(!ci.has_kbd);
    }
    // wTotalLength larger than the buffer: the buffer wins.
    {
        uint8_t b[sizeof kbd_cfg];
        memcpy(b, kbd_cfg, sizeof b);
        b[2] = 0xFF;
        b[3] = 0x03;
        CHECK(usb_desc_config(b, sizeof b, &ci));
        CHECK(ci.has_kbd);
    }
    // Bad headers.
    {
        uint8_t b[sizeof kbd_cfg];
        memcpy(b, kbd_cfg, sizeof b);
        b[2] = 8;
        b[3] = 0;
        CHECK(!usb_desc_config(b, sizeof b, &ci));  // wTotalLength < 9
        memcpy(b, kbd_cfg, sizeof b);
        b[1] = 1;
        CHECK(!usb_desc_config(b, sizeof b, &ci));  // not a configuration
        CHECK(!usb_desc_config(kbd_cfg, 8, &ci));   // too short
    }
    // Random garbage never crashes, and any keyboard found is a sane IN endpoint.
    {
        uint32_t seed = 12345;
        for (int n = 0; n < 5000; n++) {
            uint8_t b[96];
            size_t len = (size_t)(seed % sizeof b);
            for (size_t i = 0; i < sizeof b; i++) {
                seed = seed * 1103515245u + 12345u;
                b[i] = (uint8_t)(seed >> 16);
            }
            b[1] = 2;
            if (usb_desc_config(b, len, &ci) && ci.has_kbd)
                CHECK((ci.kbd_ep.addr & 0x80) && ci.kbd_ep.mps);
        }
    }

    struct usb_device_info di;
    static const uint8_t dev[18] = { 18, 1, 0x00, 0x02, 0, 0, 0, 64, 0x6D, 0x04, 0x1C, 0xC3, 0, 1, 1, 2, 0, 1 };
    CHECK(usb_desc_device(dev, sizeof dev, &di));
    CHECK_EQ(di.vid, 0x046D);
    CHECK_EQ(di.pid, 0xC31C);
    CHECK_EQ(di.dev_class, 0);
    CHECK_EQ(di.mps0, 64);
    static const uint8_t hubdev[18] = { 18, 1, 0x00, 0x02, 9, 0, 1, 64, 0x09, 0x04, 0x02, 0x00, 0, 1, 0, 0, 0, 1 };
    CHECK(usb_desc_device(hubdev, sizeof hubdev, &di));
    CHECK_EQ(di.dev_class, 9);
    CHECK_EQ(di.protocol, 1);
    CHECK(!usb_desc_device(dev, 8, &di));
    {
        uint8_t b[18];
        memcpy(b, dev, sizeof b);
        b[1] = 2;
        CHECK(!usb_desc_device(b, sizeof b, &di));
    }

    struct usb_hub_desc hd;
    static const uint8_t hub4[9] = { 9, 0x29, 4, 0x09, 0x00, 50, 100, 0, 0xFF };
    CHECK(usb_desc_hub(hub4, sizeof hub4, &hd));
    CHECK_EQ(hd.ports, 4);
    CHECK_EQ(hd.ttt, 0);
    CHECK_EQ(hd.pwr_good_ms, 100);
    static const uint8_t hub20[9] = { 9, 0x29, 20, 0x60, 0x00, 1, 100, 0, 0xFF };
    CHECK(usb_desc_hub(hub20, sizeof hub20, &hd));
    CHECK_EQ(hd.ports, 15);  // route strings address at most 15 ports
    CHECK_EQ(hd.ttt, 3);
    CHECK_EQ(hd.pwr_good_ms, 2);
    CHECK(!usb_desc_hub(hub4, 6, &hd));
    {
        uint8_t b[9];
        memcpy(b, hub4, sizeof b);
        b[1] = 0x2A;
        CHECK(!usb_desc_hub(b, sizeof b, &hd));
    }

    // First interface's class, for devices whose class lives in the interfaces.
    CHECK(usb_desc_config(kbd_cfg, sizeof kbd_cfg, &ci));
    CHECK(ci.has_iface);
    CHECK_EQ(ci.iface_class, 3);
    CHECK_EQ(ci.iface_sub, 1);
    CHECK_EQ(ci.iface_proto, 1);
    {
        // Mass storage (8/6/0x50, bulk endpoints) then a second interface that must not win.
        static const uint8_t msc_cfg[] = {
            9, 2, 41, 0, 2, 1, 0, 0x80, 50,
            9, 4, 0, 0, 2, 8, 6, 0x50, 0, 7, 5, 0x81, 2, 0, 2, 0, 7, 5, 0x02, 2, 0, 2, 0,
            9, 4, 1, 0, 0, 0xFF, 0, 0, 0,
        };
        CHECK(usb_desc_config(msc_cfg, sizeof msc_cfg, &ci));
        CHECK(ci.has_iface);
        CHECK_EQ(ci.iface_class, 8);
        CHECK_EQ(ci.iface_sub, 6);
        CHECK_EQ(ci.iface_proto, 0x50);
        CHECK(!ci.has_kbd && !ci.has_mouse && !ci.has_hub);
        static const uint8_t bare_cfg[] = { 9, 2, 9, 0, 0, 1, 0, 0x80, 50 };
        CHECK(usb_desc_config(bare_cfg, sizeof bare_cfg, &ci));
        CHECK(!ci.has_iface);
    }

    // String indexes from the device descriptor.
    CHECK(usb_desc_device(dev, sizeof dev, &di));
    CHECK_EQ(di.i_manufacturer, 1);
    CHECK_EQ(di.i_product, 2);
    CHECK_EQ(di.sub_class, 0);

    // String descriptors: UTF-16LE to ASCII.
    {
        char s[32];
        static const uint8_t qemu[] = { 10, 3, 'Q', 0, 'E', 0, 'M', 0, 'U', 0 };
        CHECK(usb_desc_string(qemu, sizeof qemu, s, sizeof s));
        CHECK_STR(s, "QEMU");
        // Non-ASCII becomes '?'; padding spaces at either end are trimmed.
        static const uint8_t odd[] = { 14, 3, ' ', 0, 0xE9, 0, 'x', 0, 0x00, 0x4E, ' ', 0, ' ', 0 };
        CHECK(usb_desc_string(odd, sizeof odd, s, sizeof s));
        CHECK_STR(s, "?x?");
        // Odd bLength: the dangling byte is dropped. Shorter buffer than bLength: cut there.
        static const uint8_t trunc[] = { 9, 3, 'a', 0, 'b', 0, 'c', 0, 'd' };
        CHECK(usb_desc_string(trunc, sizeof trunc, s, sizeof s));
        CHECK_STR(s, "abc");
        CHECK(usb_desc_string(trunc, 6, s, sizeof s));
        CHECK_STR(s, "ab");
        // Output capacity: always NUL-terminated.
        CHECK(usb_desc_string(qemu, sizeof qemu, s, 3));
        CHECK_STR(s, "QE");
        // Control characters and NULs are not copied as-is.
        static const uint8_t ctl[] = { 8, 3, 'a', 0, 0, 0, '\n', 0 };
        CHECK(usb_desc_string(ctl, sizeof ctl, s, sizeof s));
        CHECK_STR(s, "a");
        // Wrong type, too short, empty.
        static const uint8_t bad[] = { 4, 2, 'a', 0 };
        CHECK(!usb_desc_string(bad, sizeof bad, s, sizeof s));
        CHECK(!usb_desc_string(qemu, 1, s, sizeof s));
        static const uint8_t blank[] = { 6, 3, ' ', 0, ' ', 0 };
        CHECK(!usb_desc_string(blank, sizeof blank, s, sizeof s));
        CHECK(!usb_desc_string(qemu, sizeof qemu, s, 0));
        // Language list: the first LANGID.
        static const uint8_t langs[] = { 6, 3, 0x09, 0x04, 0x07, 0x04 };
        CHECK_EQ(usb_desc_langid(langs, sizeof langs), 0x0409);
        CHECK_EQ(usb_desc_langid(langs, 3), 0);
        CHECK_EQ(usb_desc_langid(bad, sizeof bad), 0);
    }

    // Class names (callers pass the interface class when the device class is 0x00 or 0xEF).
    CHECK_STR(usb_class_name(0x09, 0, 0), "Hub");
    CHECK_STR(usb_class_name(0x08, 6, 0x50), "Mass storage");
    CHECK_STR(usb_class_name(0x03, 1, 1), "HID");
    CHECK_STR(usb_class_name(0xE0, 1, 1), "Bluetooth");
    CHECK_STR(usb_class_name(0xE0, 2, 1), "Wireless");
    CHECK_STR(usb_class_name(0xFF, 0, 0), "Vendor specific");
    CHECK(usb_class_name(0x42, 0, 0) == NULL);  // unknown: the caller prints the number
    CHECK(usb_class_name(0x00, 0, 0) == NULL);

    TEST_MAIN_END();
}
