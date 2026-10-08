#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define USB_CONFIG_MAX 1024  // bytes of configuration descriptor we read

struct usb_device_info {
    uint8_t dev_class, protocol, mps0;
    uint16_t vid, pid;
};

struct usb_ep_info {
    uint8_t addr;      // with the IN bit (0x80)
    uint16_t mps;      // wMaxPacketSize bits 10:0
    uint8_t interval;  // raw bInterval
};

struct usb_config_info {
    uint8_t value;            // bConfigurationValue
    bool has_kbd;             // first boot keyboard interface (3/1/1), alt setting 0
    uint8_t kbd_iface;
    struct usb_ep_info kbd_ep;
    bool has_hub;             // first hub interface (class 9), alt setting 0
    struct usb_ep_info hub_ep;
};

struct usb_hub_desc {
    uint8_t ports;            // capped at 15
    uint8_t ttt;              // TT think time code (wHubCharacteristics bits 6:5)
    uint16_t pwr_good_ms;
};

// Pure, bounds-checked parsers. Each returns false when the header is unusable.
bool usb_desc_device(const uint8_t *b, size_t len, struct usb_device_info *out);
// Walks min(len, wTotalLength) bytes. Only interrupt IN endpoints with a
// non-zero packet size count. A truncated tail is ignored.
bool usb_desc_config(const uint8_t *b, size_t len, struct usb_config_info *out);
bool usb_desc_hub(const uint8_t *b, size_t len, struct usb_hub_desc *out);
