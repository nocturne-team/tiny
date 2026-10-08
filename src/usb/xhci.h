#pragma once
#include <stdbool.h>
#include <stdint.h>
#include "info/pci.h"

#define XHCI_MAX_SLOTS 16
#define XHCI_FREE_PAGES 96
#define XHCI_CTRL_MAX 2048    // control-transfer data lives at buf[0 .. 2047]
#define XHCI_REPORT_OFF 2048  // interrupt reports land at buf[2048 ..]
#define XHCI_REPORT_MAX 1024

// Completion codes we look at (0 is never sent by hardware: we use it for "timed out").
#define XHCI_CC_SUCCESS 1
#define XHCI_CC_STALL 6
#define XHCI_CC_SHORT 13

static inline bool xhci_ok(uint8_t cc) { return cc == XHCI_CC_SUCCESS || cc == XHCI_CC_SHORT; }

struct xhci_trb {
    uint64_t param;
    uint32_t status, control;
};

// One 4 KiB segment: 255 usable TRBs plus a Link TRB back to the start (event ring: 256, no link).
struct xhci_ring {
    volatile struct xhci_trb *trb;
    uint64_t phys;
    uint32_t idx;    // producer: next TRB to fill; event ring: next TRB to read
    uint32_t cycle;
};

struct xhci;

// Where a device sits, for Address Device.
struct xhci_dev_addr {
    uint8_t root_port, speed;
    uint32_t route;
    uint8_t tt_slot, tt_port;  // LS/FS device behind a HS hub: that hub's slot and port
};

struct xhci_hub_info {
    uint8_t ports, ttt;
};

// One addressed device slot with EP0 and at most one interrupt IN endpoint.
struct xhci_dev {
    struct xhci *x;
    uint8_t slot;              // 0 when closed
    uint8_t speed, root_port;
    uint32_t route;
    uint16_t mps0;
    volatile uint8_t *out_ctx, *in_ctx;
    uint64_t out_phys, in_phys;
    struct xhci_ring ep0, intr;
    uint8_t intr_dci;          // 0 until xhci_configure succeeds
    uint16_t intr_len;         // bytes requested per report
    uint8_t *buf;              // 4 KiB page, see XHCI_CTRL_MAX / XHCI_REPORT_OFF
    uint64_t buf_phys;
    // Set by xhci_poll when the queued interrupt transfer completes.
    bool intr_done;
    uint8_t intr_cc;
    uint16_t intr_actual;
};

struct xhci {
    char name[12];             // "bb:dd.f"
    volatile uint8_t *cap, *op, *rt, *db;
    uint64_t mmio_len;
    uint32_t xecp;             // byte offset of the extended capabilities, 0 if none
    uint8_t max_ports, max_slots, csz;
    bool dead;                 // HSE/HCE seen or the controller stopped answering
    volatile uint64_t *dcbaa;
    uint64_t dcbaa_phys;
    struct xhci_ring cmd, evt;
    uint8_t port_major[256];   // USB major revision per root port (2 or 3), 0 if unknown
    uint64_t port_changed[4];  // root ports with a Port Status Change event pending
    struct xhci_dev *slots[XHCI_MAX_SLOTS + 1];
    uint64_t free_pages[XHCI_FREE_PAGES];
    unsigned free_count;
    // The one synchronous command or control transfer in flight.
    uint64_t wait_trb[3];
    int wait_n;
    bool wait_done;
    uint8_t wait_cc, wait_slot;
};

// Maps BAR0, takes the controller from the firmware, resets and starts it. Logs why on failure.
bool xhci_init(struct xhci *x, const struct pci_device *pd);
// Drains the event ring: completes waits, flags interrupt transfers, records port changes.
void xhci_poll(struct xhci *x);
// Next root port with a pending status change (cleared as it is returned).
bool xhci_port_take_change(struct xhci *x, uint8_t *port);

// Task 6:
// Reads PORTSC and clears its change bits. Returns the value read.
uint32_t xhci_port_status(struct xhci *x, uint8_t port);
// Resets/enables a connected root port; *speed gets the xHCI speed ID.
bool xhci_port_reset(struct xhci *x, uint8_t port, uint8_t *speed);
// Enable Slot + Address Device (+ EP0 packet size fix-up for FS). False: d is closed.
bool xhci_dev_open(struct xhci *x, struct xhci_dev *d, const struct xhci_dev_addr *a);
// Disable Slot and recycle the device's pages. Safe on a closed device.
void xhci_dev_close(struct xhci_dev *d);
// Synchronous control transfer; data in/out at d->buf. Returns the completion code (0 = timeout).
uint8_t xhci_control(struct xhci_dev *d, uint8_t reqtype, uint8_t req, uint16_t value,
                     uint16_t index, uint16_t len);
// Adds the interrupt IN endpoint (and hub fields when hub != NULL) with Configure Endpoint.
bool xhci_configure(struct xhci_dev *d, uint8_t ep_addr, uint16_t mps, uint8_t binterval,
                    const struct xhci_hub_info *hub);
// Queues one interrupt transfer of d->intr_len bytes into d->buf + XHCI_REPORT_OFF.
void xhci_intr_queue(struct xhci_dev *d);
// Reset Endpoint + Set TR Dequeue Pointer, after a halt.
void xhci_ep_reset(struct xhci_dev *d, uint8_t dci);
