#include "usb/xhci_calc.h"
#include "lib/fmt.h"

#define PORTSC_RWS 0x0E00C3E0u  // PLS, PP, PIC, wake enables: written back unchanged

uint32_t xhci_portsc_ack(uint32_t v) {
    return (v & PORTSC_RWS) | (v & XHCI_PORTSC_CHANGE);
}

uint32_t xhci_portsc_set(uint32_t v, uint32_t bits) {
    return (v & PORTSC_RWS) | bits;
}

uint8_t xhci_ep_interval(uint8_t speed, uint8_t b) {
    if (speed == USB_SPEED_FS || speed == USB_SPEED_LS) {
        unsigned ms = b ? b : 1, log = 0;
        while ((2u << log) <= ms)
            log++;
        unsigned v = log + 3;
        return (uint8_t)(v > 10 ? 10 : v);
    }
    unsigned v = b ? b - 1u : 0;
    return (uint8_t)(v > 15 ? 15 : v);
}

uint8_t xhci_ep_dci(uint8_t addr) {
    uint8_t n = addr & 0xF;
    return n == 0 ? 1 : (uint8_t)(n * 2 + ((addr & 0x80) ? 1 : 0));
}

uint16_t xhci_default_mps0(uint8_t speed) {
    switch (speed) {
    case USB_SPEED_LS:
    case USB_SPEED_FS: return 8;
    case USB_SPEED_HS: return 64;
    default: return 512;
    }
}

uint32_t usb_route_append(uint32_t route, unsigned depth, uint8_t port) {
    return route | (uint32_t)(port & 0xF) << (4 * depth);
}

void usb_port_name(char *buf, size_t cap, uint8_t root_port, uint32_t route) {
    if (!cap)
        return;
    size_t n = (size_t)ksnprintf(buf, cap, "%u", root_port);
    for (unsigned t = 0; t < 5; t++) {
        unsigned port = (route >> (4 * t)) & 0xF;
        if (!port || n + 1 >= cap)
            break;
        n += (size_t)ksnprintf(buf + n, cap - n, ".%u", port);
    }
}

const char *usb_speed_name(uint8_t speed) {
    static const char *const names[] = { "?", "fs", "ls", "hs", "ss", "ss+" };
    return speed < sizeof names / sizeof names[0] ? names[speed] : "?";
}
