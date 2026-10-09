#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define USB_CONFIG_MAX 1024  // bytes of configuration descriptor we read

struct usb_device_info {
    uint8_t dev_class, sub_class, protocol, mps0;
    uint16_t vid, pid;
    uint8_t i_manufacturer, i_product;  // string descriptor indexes, 0 if none
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
    bool has_mouse;           // first boot mouse interface (3/1/2), alt setting 0
    uint8_t mouse_iface;
    struct usb_ep_info mouse_ep;
    bool has_hub;             // first hub interface (class 9), alt setting 0
    struct usb_ep_info hub_ep;
    bool has_iface;           // the first interface (alt setting 0), for its class
    uint8_t iface_class, iface_sub, iface_proto;
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
// String descriptor to printable ASCII in out[cap] (anything else becomes '?', control
// characters are dropped, surrounding spaces trimmed). False when nothing printable is left.
bool usb_desc_string(const uint8_t *b, size_t len, char *out, size_t cap);
// First LANGID of string descriptor 0, or 0 when there is none.
uint16_t usb_desc_langid(const uint8_t *b, size_t len);
// Base class name ("Mass storage"), NULL for 0x00 and unknown classes. For a device
// class of 0x00 or 0xEF (composite), pass the first interface's class instead.
const char *usb_class_name(uint8_t cls, uint8_t sub, uint8_t proto);
