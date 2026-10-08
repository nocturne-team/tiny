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
    TRB_EVALUATE_CTX = 13, TRB_RESET_EP = 14, TRB_SET_TR_DEQ = 16, TRB_NOOP_CMD = 23,
    TRB_EV_TRANSFER = 32, TRB_EV_CMD_DONE = 33, TRB_EV_PORT = 34,
};
#define TRB_CYCLE (1u << 0)
#define TRB_TC (1u << 1)
#define TRB_ISP (1u << 2)
#define TRB_IOC (1u << 5)
#define TRB_IDT (1u << 6)
#define TRB_TYPE(t) ((uint32_t)(t) << 10)
#define TRB_DIR_IN (1u << 16)
#define TRB_EP(e) ((uint32_t)(e) << 16)
#define TRB_SLOT(s) ((uint32_t)(s) << 24)
#define RING_TRBS 256

// Endpoint types (endpoint context)
#define EP_TYPE_CONTROL 4u
#define EP_TYPE_INTR_IN 7u

#define MMIO_MAX (1ull << 20)

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
