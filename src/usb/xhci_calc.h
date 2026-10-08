#pragma once
#include <stddef.h>
#include <stdint.h>

// xHCI default protocol speed IDs (PORTSC speed field, slot context speed).
enum { USB_SPEED_FS = 1, USB_SPEED_LS = 2, USB_SPEED_HS = 3, USB_SPEED_SS = 4 };

#define XHCI_PORTSC_CCS (1u << 0)   // device connected
#define XHCI_PORTSC_PED (1u << 1)   // port enabled (RW1C: writing 1 disables!)
#define XHCI_PORTSC_PR (1u << 4)    // port reset
#define XHCI_PORTSC_PP (1u << 9)    // port power
#define XHCI_PORTSC_CSC (1u << 17)  // connect status change
#define XHCI_PORTSC_PRC (1u << 21)  // port reset change
#define XHCI_PORTSC_CHANGE 0x00FE0000u
#define XHCI_PORTSC_SPEED(v) (((v) >> 10) & 0xF)

// Value to write back to PORTSC that clears its change bits and nothing else.
uint32_t xhci_portsc_ack(uint32_t portsc);
// Value to write to PORTSC that sets `bits` and keeps the preserved fields.
uint32_t xhci_portsc_set(uint32_t portsc, uint32_t bits);
// Endpoint context Interval for an interrupt endpoint's bInterval at `speed`.
uint8_t xhci_ep_interval(uint8_t speed, uint8_t binterval);
// Device context index of an endpoint address (EP0 = 1, EP n IN = 2n + 1).
uint8_t xhci_ep_dci(uint8_t ep_addr);
// EP0 max packet size to start with before the device descriptor is read.
uint16_t xhci_default_mps0(uint8_t speed);
// Route string of a device on `port` of a hub at tier `depth` (0 = hub on a root port).
uint32_t usb_route_append(uint32_t route, unsigned depth, uint8_t port);
// "2" for root port 2, "2.1" for port 1 of a hub on root port 2.
void usb_port_name(char *buf, size_t cap, uint8_t root_port, uint32_t route);
const char *usb_speed_name(uint8_t speed);

// How to restart an interrupt IN endpoint after its transfer completed with `cc`.
enum xhci_recovery {
    XHCI_RECOVER_NONE,  // success or short packet
    XHCI_RECOVER_SOFT,  // transaction error: Reset Endpoint keeping the data toggle (TSP)
    XHCI_RECOVER_HALT,  // stall, babble, anything else: reset both host and device toggles
};
enum xhci_recovery xhci_intr_recovery(uint8_t cc);
