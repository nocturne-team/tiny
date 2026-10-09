#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// A plain copy of what the USB stack found, for the USB tab.

#define USB_SUM_HCS 4
#define USB_SUM_DEVS 32

enum usb_hc_state { USB_HC_RUNNING, USB_HC_FAILED, USB_HC_STOPPED };

struct usb_hc_summary {
    char name[12];            // "bb:dd.f"
    uint16_t vendor;          // PCI vendor ID
    uint16_t version;         // HCIVERSION (BCD, 0x0120 = 1.20); 0 if never read
    uint8_t ports, usb2_ports, usb3_ports;
    enum usb_hc_state state;
};

enum usb_dev_state {
    USB_DEV_KEYBOARD,
    USB_DEV_MOUSE,
    USB_DEV_KBD_MOUSE,
    USB_DEV_HUB,
    USB_DEV_UNUSED,           // identified, nothing tiny drives
    USB_DEV_HUB_SS,           // USB 3 half of a hub: its USB 2 half is used
    USB_DEV_HUB_NESTED,       // hub behind a hub
    USB_DEV_STOPPED,          // HID whose endpoints stopped after too many errors
    USB_DEV_NO_ADDRESS,       // Enable Slot / Address Device failed
    USB_DEV_NO_DESC,          // no device descriptor
    USB_DEV_BAD_CONFIG,       // configuration descriptor unusable
    USB_DEV_SETUP_FAILED,     // HID or hub setup failed
};

struct usb_dev_summary {
    uint8_t hc;               // index into usb_summary.hc
    char port[16];            // "2" or "2.1"
    uint8_t speed;            // xHCI speed ID
    bool has_id;              // the device descriptor was read
    uint16_t vid, pid;
    uint8_t cls, sub, proto;  // device class, or the first interface's for 0x00/0xEF
    uint8_t hub_ports;
    enum usb_dev_state state;
    char manufacturer[32], product[48];  // "" when absent
};

struct usb_summary {
    bool disabled;            // nousb on the command line
    bool no_dma;              // no DMA memory: USB was never started
    size_t hc_count, dev_count;
    bool dev_truncated;       // more records than USB_SUM_DEVS
    struct usb_hc_summary hc[USB_SUM_HCS];
    struct usb_dev_summary dev[USB_SUM_DEVS];
};
