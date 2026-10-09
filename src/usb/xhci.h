#pragma once
#include <stdbool.h>
#include <stdint.h>
#include "info/pci.h"

#define XHCI_MAX_SLOTS 16
#define XHCI_FREE_PAGES 96
#define XHCI_CTRL_MAX 2048    // control-transfer data lives at buf[0 .. 2047]
#define XHCI_REPORT_OFF 2048  // interrupt reports: buffer i at buf + 2048 + 1024 * i
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

#define XHCI_MAX_INTR 2

// One interrupt IN endpoint with a single transfer in flight.
struct xhci_intr {
    struct xhci_ring ring;
    uint8_t dci;      // 0 until xhci_configure succeeds
    uint16_t len;     // bytes requested per report
    // Set by xhci_poll when the queued transfer completes.
    bool done;
    uint8_t cc;
    uint16_t actual;
};

// One addressed device slot with EP0 and up to XHCI_MAX_INTR interrupt IN endpoints.
struct xhci_dev {
    struct xhci *x;
    uint8_t slot;              // 0 when closed
    uint8_t speed, root_port;
    uint32_t route;
    uint16_t mps0;
    volatile uint8_t *out_ctx, *in_ctx;
    uint64_t out_phys, in_phys;
    struct xhci_ring ep0;
    struct xhci_intr intr[XHCI_MAX_INTR];  // interrupt IN endpoints; dci == 0 when unused
    uint8_t *buf;              // 4 KiB page, see XHCI_CTRL_MAX / xhci_report_buf
    uint64_t buf_phys;
    // A command or transfer timed out and the controller may still own our pages: never recycle them.
    bool leak;
};

static inline uint8_t *xhci_report_buf(struct xhci_dev *d, int i) {
    return d->buf + XHCI_REPORT_OFF + i * XHCI_REPORT_MAX;
}

struct xhci {
    char name[12];             // "bb:dd.f"
    volatile uint8_t *cap, *op, *rt, *db;
    uint64_t mmio_len;
    uint32_t xecp;             // byte offset of the extended capabilities, 0 if none
    uint16_t version;          // HCIVERSION, BCD
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
// Adds an interrupt IN endpoint (and hub fields when hub != NULL) with Configure Endpoint.
// Returns its index in d->intr, or -1.
int xhci_configure(struct xhci_dev *d, uint8_t ep_addr, uint16_t mps, uint8_t binterval,
                   const struct xhci_hub_info *hub);
// Queues one transfer of d->intr[i].len bytes into xhci_report_buf(d, i).
void xhci_intr_queue(struct xhci_dev *d, int i);
// Reset Endpoint + Set TR Dequeue Pointer, after a halt. keep_toggle sets TSP (soft retry
// after a transaction error, when the device's data toggle did not advance).
void xhci_ep_reset(struct xhci_dev *d, uint8_t dci, bool keep_toggle);
