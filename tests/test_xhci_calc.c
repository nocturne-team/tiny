#include "test.h"
#include "usb/xhci_calc.h"

int main(void) {
    // Interrupt endpoint interval: FS/LS bInterval in ms -> 2^n * 125 µs, n = floor(log2(ms)) + 3.
    CHECK_EQ(xhci_ep_interval(USB_SPEED_FS, 10), 6);
    CHECK_EQ(xhci_ep_interval(USB_SPEED_FS, 1), 3);
    CHECK_EQ(xhci_ep_interval(USB_SPEED_FS, 0), 3);
    CHECK_EQ(xhci_ep_interval(USB_SPEED_FS, 255), 10);
    CHECK_EQ(xhci_ep_interval(USB_SPEED_LS, 8), 6);
    // HS/SS: bInterval is already an exponent + 1.
    CHECK_EQ(xhci_ep_interval(USB_SPEED_HS, 1), 0);
    CHECK_EQ(xhci_ep_interval(USB_SPEED_HS, 4), 3);
    CHECK_EQ(xhci_ep_interval(USB_SPEED_HS, 16), 15);
    CHECK_EQ(xhci_ep_interval(USB_SPEED_HS, 0), 0);
    CHECK_EQ(xhci_ep_interval(USB_SPEED_HS, 40), 15);
    CHECK_EQ(xhci_ep_interval(USB_SPEED_SS, 8), 7);

    CHECK_EQ(xhci_ep_dci(0x00), 1);
    CHECK_EQ(xhci_ep_dci(0x01), 2);
    CHECK_EQ(xhci_ep_dci(0x81), 3);
    CHECK_EQ(xhci_ep_dci(0x82), 5);
    CHECK_EQ(xhci_ep_dci(0x8F), 31);

    CHECK_EQ(xhci_default_mps0(USB_SPEED_LS), 8);
    CHECK_EQ(xhci_default_mps0(USB_SPEED_FS), 8);
    CHECK_EQ(xhci_default_mps0(USB_SPEED_HS), 64);
    CHECK_EQ(xhci_default_mps0(USB_SPEED_SS), 512);

    // PORTSC: CCS|PED|PP|speed 4|CSC|PRC. Acknowledging writes back only the RWS
    // bits and the change bits; never PED (writing 1 would disable the port).
    uint32_t sc = 0x00221203;
    CHECK_EQ(xhci_portsc_ack(sc), 0x00220200);
    CHECK_EQ(xhci_portsc_set(sc, XHCI_PORTSC_PR), 0x00000210);
    CHECK_EQ(XHCI_PORTSC_SPEED(sc), 4);

    CHECK_EQ(usb_route_append(0, 0, 3), 0x3);
    CHECK_EQ(usb_route_append(0x3, 1, 2), 0x23);
    CHECK_EQ(usb_route_append(0, 0, 15), 0xF);

    char name[16];
    usb_port_name(name, sizeof name, 2, 0);
    CHECK_STR(name, "2");
    usb_port_name(name, sizeof name, 2, 0x1);
    CHECK_STR(name, "2.1");
    usb_port_name(name, sizeof name, 12, 0x31);
    CHECK_STR(name, "12.1.3");
    usb_port_name(name, 4, 255, 0x11111);  // truncated but terminated
    CHECK_EQ(strlen(name), 3);

    CHECK_STR(usb_speed_name(USB_SPEED_FS), "fs");
    CHECK_STR(usb_speed_name(USB_SPEED_LS), "ls");
    CHECK_STR(usb_speed_name(USB_SPEED_HS), "hs");
    CHECK_STR(usb_speed_name(USB_SPEED_SS), "ss");
    CHECK_STR(usb_speed_name(5), "ss+");
    CHECK_STR(usb_speed_name(0), "?");
    CHECK_STR(usb_speed_name(9), "?");

    // Interrupt endpoint error recovery: transaction errors keep the data toggle
    // (soft retry); stalls, babble and anything else reset both ends.
    CHECK_EQ(xhci_intr_recovery(1), XHCI_RECOVER_NONE);
    CHECK_EQ(xhci_intr_recovery(13), XHCI_RECOVER_NONE);
    CHECK_EQ(xhci_intr_recovery(4), XHCI_RECOVER_SOFT);
    CHECK_EQ(xhci_intr_recovery(6), XHCI_RECOVER_HALT);
    CHECK_EQ(xhci_intr_recovery(3), XHCI_RECOVER_HALT);
    CHECK_EQ(xhci_intr_recovery(0), XHCI_RECOVER_HALT);
    TEST_MAIN_END();
}
