#include "usb/xhci.h"
#include "arch/x86.h"
#include "info/tsc.h"
#include "lib/fmt.h"
#include "lib/serial.h"
#include "lib/string.h"
#include "mm/pages.h"
#include "mm/vmm.h"
#include "usb/xhci_calc.h"

// Capability registers
#define CAP_LENGTH_VER 0x00
#define CAP_HCSPARAMS1 0x04
#define CAP_HCSPARAMS2 0x08
#define CAP_HCCPARAMS1 0x10
#define CAP_DBOFF 0x14
#define CAP_RTSOFF 0x18
// Operational registers
#define OP_USBCMD 0x00
#define OP_USBSTS 0x04
#define OP_PAGESIZE 0x08
#define OP_CRCR 0x18
#define OP_DCBAAP 0x30
#define OP_CONFIG 0x38
#define OP_PORTSC(p) (0x400u + 0x10u * ((unsigned)(p) - 1))
#define CMD_RS (1u << 0)
#define CMD_HCRST (1u << 1)
#define STS_HCH (1u << 0)
#define STS_HSE (1u << 2)
#define STS_CNR (1u << 11)
#define STS_HCE (1u << 12)
// Runtime registers, interrupter 0
#define IR0_IMAN 0x20
#define IR0_ERSTSZ 0x28
#define IR0_ERSTBA 0x30
#define IR0_ERDP 0x38
#define ERDP_EHB (1u << 3)
// Extended capabilities
#define CAP_ID_LEGACY 1
#define CAP_ID_PROTOCOL 2
#define LEG_BIOS_OWNED (1u << 16)
#define LEG_OS_OWNED (1u << 24)
#define LEGCTL_SMI_ENABLES 0x0000E011u
#define LEGCTL_SMI_STATUS 0xE0000000u

// TRBs
enum {
    TRB_NORMAL = 1, TRB_SETUP = 2, TRB_DATA = 3, TRB_STATUS = 4, TRB_LINK = 6,
    TRB_ENABLE_SLOT = 9, TRB_DISABLE_SLOT = 10, TRB_ADDRESS_DEVICE = 11, TRB_CONFIGURE_EP = 12,
    TRB_EVALUATE_CTX = 13, TRB_RESET_EP = 14, TRB_STOP_EP = 15, TRB_SET_TR_DEQ = 16, TRB_NOOP_CMD = 23,
    TRB_EV_TRANSFER = 32, TRB_EV_CMD_DONE = 33, TRB_EV_PORT = 34,
};
#define TRB_CYCLE (1u << 0)
#define TRB_TC (1u << 1)
#define TRB_ISP (1u << 2)
#define TRB_IOC (1u << 5)
#define TRB_IDT (1u << 6)
#define TRB_TSP (1u << 9)  // Reset Endpoint: preserve the data toggle
#define TRB_TYPE(t) ((uint32_t)(t) << 10)
#define TRB_DIR_IN (1u << 16)
#define TRB_EP(e) ((uint32_t)(e) << 16)
#define TRB_SLOT(s) ((uint32_t)(s) << 24)
#define RING_TRBS 256

// Endpoint types (endpoint context)
#define EP_TYPE_CONTROL 4u
#define EP_TYPE_INTR_IN 7u

#define MMIO_MAX (1ull << 20)
#define CC_CONTEXT_STATE 19  // e.g. Stop Endpoint on an endpoint that is not running

static uint32_t rd(volatile uint8_t *base, uint32_t off) {
    return *(volatile uint32_t *)(base + off);
}

static void wr(volatile uint8_t *base, uint32_t off, uint32_t v) {
    *(volatile uint32_t *)(base + off) = v;
}

static void wr64(volatile uint8_t *base, uint32_t off, uint64_t v) {
    wr(base, off, (uint32_t)v);
    wr(base, off + 4, (uint32_t)(v >> 32));
}

static void mdelay(unsigned ms) {
    tsc_delay_us(ms * 1000ull);
}

// Polls every 10 µs until (reg & mask) == want, for up to `ms` milliseconds.
static bool wait_reg(volatile uint8_t *base, uint32_t off, uint32_t mask, uint32_t want, unsigned ms) {
    for (unsigned i = 0; i <= ms * 100; i++) {
        if ((rd(base, off) & mask) == want)
            return true;
        tsc_delay_us(10);
    }
    return false;
}

// A zeroed page: recycled from closed devices first, then from the pool.
static uint64_t get_page(struct xhci *x, void **virt) {
    uint64_t phys = x->free_count ? x->free_pages[--x->free_count] : page_alloc();
    if (!phys)
        return 0;
    *virt = phys_virt(phys);
    memset(*virt, 0, 4096);
    return phys;
}

static void put_page(struct xhci *x, uint64_t phys) {
    if (phys && x->free_count < XHCI_FREE_PAGES)
        x->free_pages[x->free_count++] = phys;
}

static bool ring_init(struct xhci *x, struct xhci_ring *r, bool link) {
    void *v;
    r->phys = get_page(x, &v);
    if (!r->phys)
        return false;
    r->trb = v;
    r->idx = 0;
    r->cycle = 1;
    if (link) {
        r->trb[RING_TRBS - 1].param = r->phys;
        r->trb[RING_TRBS - 1].control = TRB_TYPE(TRB_LINK) | TRB_TC;  // owned by us until we wrap
    }
    return true;
}

// Writes one TRB at the enqueue pointer (cycle bit last) and returns its physical address.
static uint64_t ring_push(struct xhci_ring *r, uint64_t param, uint32_t status, uint32_t control) {
    volatile struct xhci_trb *t = &r->trb[r->idx];
    uint64_t phys = r->phys + (uint64_t)r->idx * sizeof(struct xhci_trb);
    t->param = param;
    t->status = status;
    wmb();
    t->control = (control & ~TRB_CYCLE) | r->cycle;
    if (++r->idx == RING_TRBS - 1) {
        wmb();
        r->trb[RING_TRBS - 1].control = TRB_TYPE(TRB_LINK) | TRB_TC | r->cycle;
        r->idx = 0;
        r->cycle ^= 1;
    }
    return phys;
}

static void doorbell(struct xhci *x, uint8_t slot, uint8_t target) {
    wmb();
    wr(x->db, 4u * slot, target);
}

static void handle_event(struct xhci *x, volatile struct xhci_trb *e) {
    uint32_t control = e->control;
    unsigned type = (control >> 10) & 0x3F;
    uint8_t cc = (uint8_t)(e->status >> 24), slot = (uint8_t)(control >> 24);
    uint64_t ptr = e->param;

    if (type == TRB_EV_PORT) {
        uint8_t port = (uint8_t)(ptr >> 24);
        if (port)
            x->port_changed[port >> 6] |= 1ull << (port & 63);
        return;
    }
    if (type != TRB_EV_CMD_DONE && type != TRB_EV_TRANSFER)
        return;
    for (int i = 0; i < x->wait_n; i++) {
        if (ptr != x->wait_trb[i] || x->wait_done)
            continue;
        // Only an error on an earlier TRB of a TD ends the wait; success waits for the last one.
        if (i < x->wait_n - 1 && xhci_ok(cc))
            return;
        x->wait_done = true;
        x->wait_cc = cc;
        x->wait_slot = slot;
        return;
    }
    if (type == TRB_EV_TRANSFER && slot && slot <= XHCI_MAX_SLOTS && x->slots[slot]) {
        struct xhci_dev *d = x->slots[slot];
        uint8_t ep = (uint8_t)((control >> 16) & 0x1F);
        if (d->intr_dci && ep == d->intr_dci) {
            uint32_t residual = e->status & 0xFFFFFF;
            d->intr_done = true;
            d->intr_cc = cc;
            d->intr_actual = (uint16_t)(residual < d->intr_len ? d->intr_len - residual : 0);
        }
    }
}

void xhci_poll(struct xhci *x) {
    if (x->dead)
        return;
    bool any = false;
    for (;;) {
        volatile struct xhci_trb *e = &x->evt.trb[x->evt.idx];
        if ((e->control & TRB_CYCLE) != x->evt.cycle)
            break;
        handle_event(x, e);
        any = true;
        if (++x->evt.idx == RING_TRBS) {
            x->evt.idx = 0;
            x->evt.cycle ^= 1;
        }
    }
    if (any)
        wr64(x->rt, IR0_ERDP, (x->evt.phys + (uint64_t)x->evt.idx * sizeof(struct xhci_trb)) | ERDP_EHB);
    uint32_t sts = rd(x->op, OP_USBSTS);
    if (sts == 0xFFFFFFFF || (sts & (STS_HSE | STS_HCE))) {
        x->dead = true;
        klog("xhci: %s: controller error (USBSTS %08x), stopped", x->name, sts);
    }
}

// Waits for the TRBs in x->wait_trb. Returns their completion code, 0 on timeout.
static uint8_t wait_event(struct xhci *x, unsigned ms) {
    for (unsigned i = 0; i <= ms * 100 && !x->wait_done && !x->dead; i++) {
        xhci_poll(x);
        if (!x->wait_done)
            tsc_delay_us(10);
    }
    x->wait_n = 0;
    return x->wait_done ? x->wait_cc : 0;
}

static uint8_t command(struct xhci *x, uint64_t param, uint32_t control, uint8_t *slot) {
    if (x->dead)
        return 0;
    x->wait_done = false;
    x->wait_trb[0] = ring_push(&x->cmd, param, 0, control);
    x->wait_n = 1;
    doorbell(x, 0, 0);
    uint8_t cc = wait_event(x, 1000);
    if (slot)
        *slot = x->wait_slot;
    if (cc != XHCI_CC_SUCCESS)
        klog("xhci: %s: command %u failed (cc %u)", x->name, (unsigned)(control >> 10) & 0x3F, cc);
    return cc;
}

static uint32_t cap_next(struct xhci *x, uint32_t off) {
    uint32_t next = (rd(x->cap, off) >> 8) & 0xFF;
    return next ? off + next * 4 : 0;
}

// Offset of the first extended capability with `id` at or after `off`, 0 if none.
static uint32_t cap_find(struct xhci *x, uint32_t off, uint8_t id) {
    for (int n = 0; off && off + 16 <= x->mmio_len && n < 64; n++, off = cap_next(x, off))
        if ((rd(x->cap, off) & 0xFF) == id)
            return off;
    return 0;
}

static bool map_bar(struct xhci *x, const struct pci_device *pd) {
    uint8_t b = pd->bus, dv = pd->dev, f = pd->fn;
    uint32_t cmd = pci_config_read(b, dv, f, 0x04) & 0xFFFF;
    pci_config_write(b, dv, f, 0x04, cmd & ~0x2u);  // memory decode off while sizing BAR0
    uint32_t lo = pci_config_read(b, dv, f, 0x10);
    uint32_t hi = ((lo >> 1) & 3) == 2 ? pci_config_read(b, dv, f, 0x14) : 0;
    pci_config_write(b, dv, f, 0x10, 0xFFFFFFFF);
    uint64_t size = pci_bar_size(pci_config_read(b, dv, f, 0x10));
    pci_config_write(b, dv, f, 0x10, lo);
    pci_config_write(b, dv, f, 0x04, cmd | 0x6u);  // memory space + bus master
    uint64_t base = pci_bar_base(lo, hi);
    if (!base || size < 0x1000)
        return false;
    x->mmio_len = size < MMIO_MAX ? size : MMIO_MAX;
    x->cap = vmm_map_mmio(base, x->mmio_len);
    return x->cap != NULL;
}

static void handoff(struct xhci *x) {
    uint32_t off = cap_find(x, x->xecp, CAP_ID_LEGACY);
    if (!off)
        return;
    uint32_t v = rd(x->cap, off);
    wr(x->cap, off, v | LEG_OS_OWNED);
    if (!(v & LEG_BIOS_OWNED) || wait_reg(x->cap, off, LEG_BIOS_OWNED, 0, 1000)) {
        klog("xhci: %s: bios handoff ok", x->name);
    } else {
        wr(x->cap, off, (rd(x->cap, off) & ~LEG_BIOS_OWNED) | LEG_OS_OWNED);
        klog("xhci: %s: bios handoff forced", x->name);
    }
    uint32_t ctl = rd(x->cap, off + 4);
    wr(x->cap, off + 4, (ctl & ~LEGCTL_SMI_ENABLES) | LEGCTL_SMI_STATUS);
}

// Intel 7/8/9-series PCHs can route their USB ports to EHCI; send them all to xHCI.
static void intel_route(struct xhci *x, const struct pci_device *pd) {
    static const uint16_t ids[] = { 0x1E31, 0x8C31, 0x9C31, 0x8CB1, 0x9CB1, 0x8D31 };
    if (pd->vendor != 0x8086)
        return;
    for (size_t i = 0; i < sizeof ids / sizeof ids[0]; i++) {
        if (pd->device != ids[i])
            continue;
        pci_config_write(pd->bus, pd->dev, pd->fn, 0xD8, pci_config_read(pd->bus, pd->dev, pd->fn, 0xDC));
        pci_config_write(pd->bus, pd->dev, pd->fn, 0xD0, pci_config_read(pd->bus, pd->dev, pd->fn, 0xD4));
        klog("xhci: %s: switched Intel shared ports to xHCI", x->name);
        return;
    }
}

static bool reset(struct xhci *x) {
    wr(x->op, OP_USBCMD, rd(x->op, OP_USBCMD) & ~CMD_RS);
    if (!wait_reg(x->op, OP_USBSTS, STS_HCH, STS_HCH, 1000))
        goto fail;
    wr(x->op, OP_USBCMD, CMD_HCRST);
    mdelay(1);  // some Intel controllers hang if touched right after HCRST
    if (!wait_reg(x->op, OP_USBCMD, CMD_HCRST, 0, 1000) || !wait_reg(x->op, OP_USBSTS, STS_CNR, 0, 1000))
        goto fail;
    return true;
fail:
    klog("xhci: %s: reset timed out, skipped", x->name);
    return false;
}

static void read_protocols(struct xhci *x) {
    for (uint32_t off = cap_find(x, x->xecp, CAP_ID_PROTOCOL); off;
         off = cap_find(x, cap_next(x, off), CAP_ID_PROTOCOL)) {
        uint8_t major = (uint8_t)(rd(x->cap, off) >> 24);
        uint32_t ports = rd(x->cap, off + 8);
        unsigned first = ports & 0xFF, count = (ports >> 8) & 0xFF;
        for (unsigned p = first; p < first + count && p <= x->max_ports; p++)
            x->port_major[p] = major;
    }
}

bool xhci_init(struct xhci *x, const struct pci_device *pd) {
    memset(x, 0, sizeof *x);
    ksnprintf(x->name, sizeof x->name, "%02x:%02x.%x", pd->bus, pd->dev, pd->fn);
    if (!map_bar(x, pd)) {
        klog("xhci: %s: BAR0 unusable, skipped", x->name);
        return false;
    }
    uint32_t cap0 = rd(x->cap, CAP_LENGTH_VER);
    if (cap0 == 0xFFFFFFFF) {
        klog("xhci: %s: registers read all ones, skipped", x->name);
        return false;
    }
    x->op = x->cap + (cap0 & 0xFF);
    uint32_t hcs1 = rd(x->cap, CAP_HCSPARAMS1), hcs2 = rd(x->cap, CAP_HCSPARAMS2);
    uint32_t hcc1 = rd(x->cap, CAP_HCCPARAMS1);
    x->rt = x->cap + (rd(x->cap, CAP_RTSOFF) & ~0x1Fu);
    x->db = x->cap + (rd(x->cap, CAP_DBOFF) & ~0x3u);
    x->xecp = (hcc1 >> 16) * 4;
    x->max_ports = (uint8_t)(hcs1 >> 24);
    uint8_t slots = hcs1 & 0xFF;
    x->max_slots = slots < XHCI_MAX_SLOTS ? slots : XHCI_MAX_SLOTS;
    x->csz = (hcc1 & 4) ? 64 : 32;
    unsigned sp = ((hcs2 >> 21) & 0x1F) << 5 | hcs2 >> 27;
    uint16_t ver = (uint16_t)(cap0 >> 16);
    klog("xhci: %s v%x.%02x, %u ports, %u slots, ctx %u", x->name, ver >> 8, ver & 0xFF,
         x->max_ports, slots, x->csz);

    handoff(x);
    intel_route(x, pd);
    if (!reset(x))
        return false;
    if (!(rd(x->op, OP_PAGESIZE) & 1) || !x->max_slots || sp > 512) {
        klog("xhci: %s: unsupported (page size, slots or %u scratchpads), skipped", x->name, sp);
        return false;
    }

    void *v;
    if (!(x->dcbaa_phys = get_page(x, &v)))
        goto nomem;
    x->dcbaa = v;
    if (sp) {
        uint64_t arr_phys = get_page(x, &v);
        if (!arr_phys)
            goto nomem;
        uint64_t *arr = v;
        for (unsigned i = 0; i < sp; i++)
            if (!(arr[i] = get_page(x, &v)))
                goto nomem;
        x->dcbaa[0] = arr_phys;
    }
    if (!ring_init(x, &x->cmd, true) || !ring_init(x, &x->evt, false))
        goto nomem;
    uint64_t erst_phys = get_page(x, &v);
    if (!erst_phys)
        goto nomem;
    volatile struct xhci_trb *erst = v;  // ERST entry: base, size; same 16-byte shape as a TRB
    erst[0].param = x->evt.phys;
    erst[0].status = RING_TRBS;

    wr(x->op, OP_CONFIG, x->max_slots);
    wr64(x->op, OP_DCBAAP, x->dcbaa_phys);
    wr64(x->op, OP_CRCR, x->cmd.phys | 1);
    wr(x->rt, IR0_ERSTSZ, 1);
    wr64(x->rt, IR0_ERDP, x->evt.phys);
    wr64(x->rt, IR0_ERSTBA, erst_phys);
    wr(x->rt, IR0_IMAN, 1);  // acknowledge IP; IE stays 0, we poll
    wr(x->op, OP_USBCMD, CMD_RS);
    if (!wait_reg(x->op, OP_USBSTS, STS_HCH, 0, 100)) {
        klog("xhci: %s: did not start, skipped", x->name);
        return false;
    }
    read_protocols(x);
    for (unsigned p = 1; p <= x->max_ports; p++) {  // power every port (no-op without PPC)
        uint32_t sc = rd(x->op, OP_PORTSC(p));
        if (!(sc & XHCI_PORTSC_PP))
            wr(x->op, OP_PORTSC(p), xhci_portsc_set(sc, XHCI_PORTSC_PP));
    }
    if (command(x, 0, TRB_TYPE(TRB_NOOP_CMD), NULL) != XHCI_CC_SUCCESS) {
        wr(x->op, OP_USBCMD, 0);
        x->dead = true;
        klog("xhci: %s: command ring not responding, skipped", x->name);
        return false;
    }
    klog("xhci: %s running", x->name);
    return true;
nomem:
    klog("xhci: %s: out of DMA memory, skipped", x->name);
    return false;
}

bool xhci_port_take_change(struct xhci *x, uint8_t *port) {
    for (unsigned w = 0; w < 4; w++) {
        if (!x->port_changed[w])
            continue;
        unsigned b = (unsigned)__builtin_ctzll(x->port_changed[w]);
        x->port_changed[w] &= ~(1ull << b);
        *port = (uint8_t)(w * 64 + b);
        return true;
    }
    return false;
}

uint32_t xhci_port_status(struct xhci *x, uint8_t port) {
    uint32_t sc = rd(x->op, OP_PORTSC(port));
    if (sc & XHCI_PORTSC_CHANGE)
        wr(x->op, OP_PORTSC(port), xhci_portsc_ack(sc));
    return sc;
}

bool xhci_port_reset(struct xhci *x, uint8_t port, uint8_t *speed) {
    uint32_t off = OP_PORTSC(port);
    if (x->port_major[port] != 3) {  // USB 2 ports need a reset; USB 3 ports train by themselves
        wr(x->op, off, xhci_portsc_set(rd(x->op, off), XHCI_PORTSC_PR));
        if (!wait_reg(x->op, off, XHCI_PORTSC_PRC, XHCI_PORTSC_PRC, 500))
            return false;
    }
    if (!wait_reg(x->op, off, XHCI_PORTSC_PED, XHCI_PORTSC_PED, 500))
        return false;
    *speed = (uint8_t)XHCI_PORTSC_SPEED(xhci_port_status(x, port));
    return true;
}

// Input context: [0] control, [1] slot, [1 + dci] endpoints. Output context: [0] slot, [dci].
static volatile uint32_t *in_ctx(struct xhci_dev *d, unsigned i) {
    return (volatile uint32_t *)(d->in_ctx + i * d->x->csz);
}

static volatile uint32_t *out_ctx(struct xhci_dev *d, unsigned i) {
    return (volatile uint32_t *)(d->out_ctx + i * d->x->csz);
}

bool xhci_dev_open(struct xhci *x, struct xhci_dev *d, const struct xhci_dev_addr *a) {
    memset(d, 0, sizeof *d);
    d->x = x;
    d->speed = a->speed;
    d->root_port = a->root_port;
    d->route = a->route;
    uint8_t slot = 0;
    if (command(x, 0, TRB_TYPE(TRB_ENABLE_SLOT), &slot) != XHCI_CC_SUCCESS)
        return false;
    if (!slot || slot > x->max_slots) {
        klog("xhci: %s: bad slot id %u", x->name, slot);
        return false;
    }
    d->slot = slot;
    void *v;
    if (!(d->out_phys = get_page(x, &v)))
        goto fail;
    d->out_ctx = v;
    if (!(d->in_phys = get_page(x, &v)))
        goto fail;
    d->in_ctx = v;
    if (!(d->buf_phys = get_page(x, &v)))
        goto fail;
    d->buf = v;
    if (!ring_init(x, &d->ep0, true))
        goto fail;
    x->dcbaa[slot] = d->out_phys;
    x->slots[slot] = d;

    d->mps0 = xhci_default_mps0(a->speed);
    volatile uint32_t *ctl = in_ctx(d, 0), *s = in_ctx(d, 1), *ep = in_ctx(d, 2);
    ctl[1] = 0x3;  // add slot + EP0
    s[0] = (a->route & 0xFFFFF) | (uint32_t)a->speed << 20 | 1u << 27;
    s[1] = (uint32_t)a->root_port << 16;
    s[2] = a->tt_slot | (uint32_t)a->tt_port << 8;
    ep[1] = 3u << 1 | EP_TYPE_CONTROL << 3 | (uint32_t)d->mps0 << 16;
    ep[2] = (uint32_t)d->ep0.phys | 1;
    ep[3] = (uint32_t)(d->ep0.phys >> 32);
    ep[4] = 8;
    if (command(x, d->in_phys, TRB_TYPE(TRB_ADDRESS_DEVICE) | TRB_SLOT(slot), NULL) != XHCI_CC_SUCCESS)
        goto fail;
    mdelay(2);  // SetAddress recovery time

    if (a->speed == USB_SPEED_FS) {  // FS EP0 may be 8, 16, 32 or 64 bytes
        if (!xhci_ok(xhci_control(d, 0x80, 6, 0x0100, 0, 8)))
            goto fail;
        uint8_t mps = d->buf[7];
        if (mps != d->mps0 && (mps == 16 || mps == 32 || mps == 64)) {
            d->mps0 = mps;
            ctl[0] = 0;
            ctl[1] = 0x2;  // evaluate EP0 only
            ep[1] = (ep[1] & 0xFFFF) | (uint32_t)mps << 16;
            if (command(x, d->in_phys, TRB_TYPE(TRB_EVALUATE_CTX) | TRB_SLOT(slot), NULL) != XHCI_CC_SUCCESS)
                goto fail;
        }
    }
    return true;
fail:
    xhci_dev_close(d);
    return false;
}

void xhci_dev_close(struct xhci_dev *d) {
    struct xhci *x = d->x;
    if (!x)
        return;
    // Pages are recycled only once the controller has let go of the slot; otherwise
    // a late DMA could land in another device's ring. Leaking is safe: the pool is bounded.
    bool released = !d->slot || x->dead;
    if (d->slot) {
        if (!x->dead)
            released = command(x, 0, TRB_TYPE(TRB_DISABLE_SLOT) | TRB_SLOT(d->slot), NULL) == XHCI_CC_SUCCESS;
        x->dcbaa[d->slot] = 0;
        x->slots[d->slot] = NULL;
    }
    if (released && !d->leak) {
        put_page(x, d->out_phys);
        put_page(x, d->in_phys);
        put_page(x, d->buf_phys);
        put_page(x, d->ep0.phys);
        put_page(x, d->intr.phys);
    }
    memset(d, 0, sizeof *d);
}

static void set_dequeue(struct xhci_dev *d, uint8_t dci) {
    struct xhci_ring *r = dci == 1 ? &d->ep0 : &d->intr;
    command(d->x, (r->phys + (uint64_t)r->idx * sizeof(struct xhci_trb)) | r->cycle,
            TRB_TYPE(TRB_SET_TR_DEQ) | TRB_EP(dci) | TRB_SLOT(d->slot), NULL);
}

// A TD that never completed: stop the endpoint and skip everything queued on it, so
// later transfers do not wait behind it and its buffer is not written later.
static void ep_abort(struct xhci_dev *d, uint8_t dci) {
    uint8_t cc = command(d->x, 0, TRB_TYPE(TRB_STOP_EP) | TRB_EP(dci) | TRB_SLOT(d->slot), NULL);
    if (cc != XHCI_CC_SUCCESS && cc != CC_CONTEXT_STATE) {
        d->leak = true;  // could not take the TD back
        return;
    }
    set_dequeue(d, dci);
}

uint8_t xhci_control(struct xhci_dev *d, uint8_t reqtype, uint8_t req, uint16_t value,
                     uint16_t index, uint16_t len) {
    struct xhci *x = d->x;
    if (x->dead || len > XHCI_CTRL_MAX)
        return 0;
    bool in = reqtype & 0x80;
    uint64_t setup = reqtype | (uint64_t)req << 8 | (uint64_t)value << 16 |
                     (uint64_t)index << 32 | (uint64_t)len << 48;
    uint32_t trt = len ? (in ? 3u : 2u) : 0u;  // transfer type: none / OUT / IN data stage
    if (in && len)
        memset(d->buf, 0, len);
    x->wait_done = false;
    x->wait_n = 0;
    x->wait_trb[x->wait_n++] = ring_push(&d->ep0, setup, 8, TRB_TYPE(TRB_SETUP) | TRB_IDT | trt << 16);
    if (len)
        x->wait_trb[x->wait_n++] = ring_push(&d->ep0, d->buf_phys, len,
                                             TRB_TYPE(TRB_DATA) | (in ? TRB_DIR_IN : 0));
    x->wait_trb[x->wait_n++] = ring_push(&d->ep0, 0, 0,
                                         TRB_TYPE(TRB_STATUS) | TRB_IOC | (in && len ? 0 : TRB_DIR_IN));
    doorbell(x, d->slot, 1);
    uint8_t cc = wait_event(x, 1000);
    if (!cc)
        ep_abort(d, 1);  // timed out: take the TD back
    else if (!xhci_ok(cc))
        xhci_ep_reset(d, 1, false);  // stalled or failed: un-halt EP0 and skip the rest of the TD
    return cc;
}

bool xhci_configure(struct xhci_dev *d, uint8_t ep_addr, uint16_t mps, uint8_t binterval,
                    const struct xhci_hub_info *hub) {
    struct xhci *x = d->x;
    uint8_t dci = xhci_ep_dci(ep_addr);
    if (!ring_init(x, &d->intr, true))
        return false;
    memset((void *)(uintptr_t)d->in_ctx, 0, 4096);
    volatile uint32_t *ctl = in_ctx(d, 0), *s = in_ctx(d, 1), *ep = in_ctx(d, 1 + dci);
    volatile uint32_t *cur = out_ctx(d, 0);
    ctl[1] = 1u | 1u << dci;
    for (int i = 0; i < 4; i++)
        s[i] = cur[i];
    s[0] = (s[0] & ~(0x1Fu << 27)) | (uint32_t)dci << 27;  // context entries
    s[3] = 0;
    if (hub) {
        s[0] |= 1u << 26;
        s[1] = (s[1] & 0x00FFFFFF) | (uint32_t)hub->ports << 24;
        s[2] = (s[2] & ~(3u << 16)) | (uint32_t)hub->ttt << 16;
    }
    ep[0] = (uint32_t)xhci_ep_interval(d->speed, binterval) << 16;
    ep[1] = 3u << 1 | EP_TYPE_INTR_IN << 3 | (uint32_t)mps << 16;
    ep[2] = (uint32_t)d->intr.phys | 1;
    ep[3] = (uint32_t)(d->intr.phys >> 32);
    ep[4] = 8 | (uint32_t)mps << 16;  // average TRB length, max ESIT payload
    uint8_t cc = command(x, d->in_phys, TRB_TYPE(TRB_CONFIGURE_EP) | TRB_SLOT(d->slot), NULL);
    if (cc != XHCI_CC_SUCCESS) {
        if (!cc)
            d->leak = true;  // the controller may still adopt the ring; xhci_dev_close frees the rest
        return false;
    }
    d->intr_dci = dci;
    d->intr_len = mps > XHCI_REPORT_MAX ? XHCI_REPORT_MAX : mps;
    return true;
}

void xhci_intr_queue(struct xhci_dev *d) {
    d->intr_done = false;
    ring_push(&d->intr, d->buf_phys + XHCI_REPORT_OFF, d->intr_len,
              TRB_TYPE(TRB_NORMAL) | TRB_IOC | TRB_ISP);
    doorbell(d->x, d->slot, d->intr_dci);
}

void xhci_ep_reset(struct xhci_dev *d, uint8_t dci, bool keep_toggle) {
    command(d->x, 0, TRB_TYPE(TRB_RESET_EP) | (keep_toggle ? TRB_TSP : 0) | TRB_EP(dci) | TRB_SLOT(d->slot), NULL);
    set_dequeue(d, dci);
}
