#include "usb/usb.h"
#include "info/tsc.h"
#include "lib/serial.h"
#include "lib/string.h"
#include "usb/hid_kbd.h"
#include "usb/hid_mouse.h"
#include "usb/usb_desc.h"
#include "usb/xhci.h"
#include "usb/xhci_calc.h"

#define USB_MAX_HC 4
#define USB_MAX_DEVS 32
#define HUB_MAX_PORTS 15

// Standard requests (USB 2.0 ch. 9) and HID class requests.
#define REQ_GET_STATUS 0
#define REQ_CLEAR_FEATURE 1
#define REQ_SET_FEATURE 3
#define REQ_GET_DESCRIPTOR 6
#define REQ_SET_CONFIGURATION 9
#define HID_GET_PROTOCOL 0x03
#define HID_SET_IDLE 0x0A
#define HID_SET_PROTOCOL 0x0B

// DEV_IGNORED: a record for the USB tab only; its slot is closed.
enum dev_kind { DEV_OTHER, DEV_HID, DEV_HUB, DEV_IGNORED };

struct usb_dev {
    bool used;
    enum dev_kind kind;
    struct xhci_dev xd;
    char name[16];                        // "2" on a root port, "2.1" behind a hub
    struct usb_dev *parent;               // hub, or NULL on a root port
    uint8_t errors[XHCI_MAX_INTR];        // consecutive transfer errors, per endpoint
    bool stopped[XHCI_MAX_INTR];          // too many errors: endpoint no longer polled
    struct hid_kbd kbd;
    bool is_kbd, is_mouse;                // HID functions that configured successfully
    uint8_t kbd_ep, mouse_ep, hub_ep;     // their indices in xd.intr
    uint8_t nports;                       // hub
    struct usb_dev *child[HUB_MAX_PORTS + 1];
    uint16_t skip;                        // hub ports to leave alone until unplugged
    // What the USB tab shows. `state` is only meaningful for DEV_IGNORED.
    enum usb_dev_state state;
    uint8_t speed;
    bool has_id;
    struct usb_device_info di;
    uint8_t cls, sub, proto;              // device class, or the first interface's
    char manufacturer[32], product[48];
};

struct usb_hc {
    struct xhci x;
    struct usb_dev *port_dev[256];
    uint64_t skip[4];                     // root ports to leave alone until unplugged
    bool dead_handled;
    bool up;                              // xhci_init succeeded
    uint16_t vendor;
};

static struct usb_hc hcs[USB_MAX_HC];
static size_t hc_count, hc_up, kbd_count;
static uint32_t generation;  // bumped whenever the device tree changes
static struct usb_dev devs[USB_MAX_DEVS];

static void mdelay(unsigned ms) {
    tsc_delay_us(ms * 1000ull);
}

static struct usb_dev *dev_new(void) {
    for (size_t i = 0; i < USB_MAX_DEVS; i++) {
        if (!devs[i].used) {
            memset(&devs[i], 0, sizeof devs[i]);
            devs[i].used = true;
            return &devs[i];
        }
    }
    return NULL;
}

static void dev_free(struct usb_dev *u) {
    generation++;
    if (u->is_kbd)
        kbd_count--;
    xhci_dev_close(&u->xd);
    memset(u, 0, sizeof *u);
}

// Removes a device and, for a hub, everything behind it.
static void dev_remove(struct usb_dev *u) {
    for (unsigned p = 1; p <= HUB_MAX_PORTS; p++)
        if (u->child[p])
            dev_remove(u->child[p]);
    dev_free(u);
}

static struct usb_dev *dev_attach(struct xhci *x, const struct xhci_dev_addr *a, struct usb_dev *parent);

static uint8_t get_desc(struct xhci_dev *d, uint16_t type_index, uint16_t len) {
    return xhci_control(d, 0x80, REQ_GET_DESCRIPTOR, type_index, 0, len);
}

// One HID interface: boot protocol, no idle reports (either may stall on boot-only
// devices), then its interrupt endpoint. *idx gets the endpoint index.
static bool hid_iface_setup(struct usb_dev *u, uint8_t iface, const struct usb_ep_info *ep, uint8_t *idx) {
    struct xhci_dev *d = &u->xd;
    xhci_control(d, 0x21, HID_SET_PROTOCOL, 0, iface, 0);
    // A device that ignored SET_PROTOCOL would send report-protocol data that the boot
    // decoders misread (a mouse whose motion scrolls pages). If it answers GET_PROTOCOL
    // with "report", skip the interface; if it stalls, it is boot-only, which is fine.
    if (xhci_ok(xhci_control(d, 0xA1, HID_GET_PROTOCOL, 0, iface, 1)) && d->buf[0] != 0) {
        klog("usb: port %s: interface %u stays in report protocol, ignored", u->name, iface);
        return false;
    }
    xhci_control(d, 0x21, HID_SET_IDLE, 0, iface, 0);
    int i = xhci_configure(d, ep->addr, ep->mps, ep->interval, NULL);
    if (i < 0)
        return false;
    *idx = (uint8_t)i;
    xhci_intr_queue(d, i);
    return true;
}

// Keyboard and/or mouse. Succeeds when at least one of them works.
static bool hid_setup(struct usb_dev *u, const struct usb_config_info *ci) {
    if (!xhci_ok(xhci_control(&u->xd, 0x00, REQ_SET_CONFIGURATION, ci->value, 0, 0)))
        return false;
    if (ci->has_kbd && hid_iface_setup(u, ci->kbd_iface, &ci->kbd_ep, &u->kbd_ep)) {
        u->is_kbd = true;
        kbd_count++;
    }
    if (ci->has_mouse && hid_iface_setup(u, ci->mouse_iface, &ci->mouse_ep, &u->mouse_ep))
        u->is_mouse = true;
    if (!u->is_kbd && !u->is_mouse)
        return false;
    u->kind = DEV_HID;
    return true;
}

// Hub class features (USB 2.0 11.24.2).
#define HUB_PORT_RESET 4
#define HUB_PORT_POWER 8
#define HUB_C_PORT_CONNECTION 16
#define HUB_C_PORT_RESET 20
#define HUB_ST_CONNECTION (1u << 0)
#define HUB_ST_ENABLE (1u << 1)
#define HUB_ST_LOW_SPEED (1u << 9)
#define HUB_ST_HIGH_SPEED (1u << 10)
#define HUB_CH_CONNECTION (1u << 0)
#define HUB_CH_RESET (1u << 4)

static bool hub_port_status(struct xhci_dev *d, uint8_t port, uint16_t *status, uint16_t *change) {
    if (!xhci_ok(xhci_control(d, 0xA3, REQ_GET_STATUS, 0, port, 4)))
        return false;
    *status = (uint16_t)(d->buf[0] | d->buf[1] << 8);
    *change = (uint16_t)(d->buf[2] | d->buf[3] << 8);
    return true;
}

// Same idea as service_root, for port `port` of hub `h`.
static void service_hub_port(struct usb_dev *h, uint8_t port, bool debounce) {
    struct xhci_dev *d = &h->xd;
    uint16_t status, change;
    if (!port || port > h->nports)
        return;
    if (!hub_port_status(d, port, &status, &change)) {
        // Unreadable port: leave it alone until the hub is replugged, rather than
        // paying a control-transfer timeout on every status interrupt.
        if (!(h->skip & (1u << port)))
            klog("usb: port %s.%u: status unreadable, ignored", h->name, port);
        h->skip |= (uint16_t)(1u << port);
        return;
    }
    for (unsigned b = 0; b < 5; b++)  // acknowledge C_PORT_CONNECTION .. C_PORT_RESET
        if (change & (1u << b))
            xhci_control(d, 0x23, REQ_CLEAR_FEATURE, (uint16_t)(HUB_C_PORT_CONNECTION + b), port, 0);
    bool conn = status & HUB_ST_CONNECTION, replugged = change & HUB_CH_CONNECTION;
    struct usb_dev **c = &h->child[port];
    uint16_t bit = (uint16_t)(1u << port);

    if (*c && (!conn || replugged)) {
        klog("usb: port %s disconnected", (*c)->name);
        dev_remove(*c);
        *c = NULL;
    }
    if (!conn || replugged)
        h->skip &= (uint16_t)~bit;
    if (!conn || *c || (h->skip & bit))
        return;
    if (debounce)
        mdelay(100);
    xhci_control(d, 0x23, REQ_SET_FEATURE, HUB_PORT_RESET, port, 0);
    bool reset_done = false;
    for (int i = 0; i < 50 && !reset_done; i++) {
        mdelay(10);
        if (!hub_port_status(d, port, &status, &change))
            break;  // hub stopped answering: give up now, not after 50 timeouts
        reset_done = change & HUB_CH_RESET;
    }
    char name[16];
    usb_port_name(name, sizeof name, d->root_port, usb_route_append(d->route, 0, port));
    if (!reset_done || !(status & HUB_ST_ENABLE)) {
        klog("usb: port %s: reset failed", name);
        h->skip |= bit;
        return;
    }
    xhci_control(d, 0x23, REQ_CLEAR_FEATURE, HUB_C_PORT_RESET, port, 0);
    mdelay(10);  // reset recovery
    uint8_t speed = (status & HUB_ST_LOW_SPEED) ? USB_SPEED_LS
                  : (status & HUB_ST_HIGH_SPEED) ? USB_SPEED_HS : USB_SPEED_FS;
    struct xhci_dev_addr a = {
        .root_port = d->root_port,
        .speed = speed,
        .route = usb_route_append(d->route, 0, port),
    };
    if (d->speed == USB_SPEED_HS && speed != USB_SPEED_HS) {  // LS/FS behind a HS hub: its TT
        a.tt_slot = d->slot;
        a.tt_port = port;
    }
    *c = dev_attach(d->x, &a, h);
    if (!*c)
        h->skip |= bit;
}

static bool hub_setup(struct usb_dev *u, const struct usb_config_info *ci) {
    struct xhci_dev *d = &u->xd;
    struct usb_hub_desc hd;
    if (!ci->has_hub || !xhci_ok(xhci_control(d, 0x00, REQ_SET_CONFIGURATION, ci->value, 0, 0)))
        return false;
    if (!xhci_ok(xhci_control(d, 0xA0, REQ_GET_DESCRIPTOR, 0x2900, 0, 16)) ||
        !usb_desc_hub(d->buf, 16, &hd))
        return false;
    struct xhci_hub_info hi = { .ports = hd.ports, .ttt = d->speed == USB_SPEED_HS ? hd.ttt : 0 };
    int hub_ep = xhci_configure(d, ci->hub_ep.addr, ci->hub_ep.mps, ci->hub_ep.interval, &hi);
    if (hub_ep < 0)
        return false;
    u->hub_ep = (uint8_t)hub_ep;
    u->kind = DEV_HUB;
    u->nports = hd.ports;
    klog("usb: port %s %s hub, %u ports", u->name, usb_speed_name(d->speed), hd.ports);
    for (uint8_t p = 1; p <= hd.ports; p++)
        xhci_control(d, 0x23, REQ_SET_FEATURE, HUB_PORT_POWER, p, 0);
    mdelay(hd.pwr_good_ms > 100 ? hd.pwr_good_ms : 100);
    for (uint8_t p = 1; p <= hd.ports; p++)
        service_hub_port(u, p, false);
    xhci_intr_queue(d, u->hub_ep);
    return true;
}

// One string descriptor into out[cap]; out stays "" when the device has none.
static void get_string(struct xhci_dev *d, uint8_t index, uint16_t lang, char *out, size_t cap) {
    out[0] = 0;
    if (!index)
        return;
    memset(d->buf, 0, 256);
    if (xhci_ok(xhci_control(d, 0x80, REQ_GET_DESCRIPTOR, (uint16_t)(0x0300 | index), lang, 255)))
        usb_desc_string(d->buf, 255, out, cap);
}

static void read_strings(struct usb_dev *u) {
    struct xhci_dev *d = &u->xd;
    if (!u->di.i_manufacturer && !u->di.i_product)
        return;
    memset(d->buf, 0, 256);
    uint8_t cc = xhci_control(d, 0x80, REQ_GET_DESCRIPTOR, 0x0300, 0, 255);
    if (!cc)
        return;  // timed out: don't pay that again for each string
    uint16_t lang = xhci_ok(cc) ? usb_desc_langid(d->buf, 255) : 0;
    if (!lang)
        lang = 0x0409;  // English (US), what most devices have
    get_string(d, u->di.i_manufacturer, lang, u->manufacturer, sizeof u->manufacturer);
    get_string(d, u->di.i_product, lang, u->product, sizeof u->product);
    if (u->manufacturer[0] || u->product[0])
        klog("usb: port %s: \"%s\" \"%s\"", u->name, u->manufacturer, u->product);
}

// Addresses and identifies the device at `a`. A device tiny does not use, or could
// not set up, stays as a DEV_IGNORED record (slot closed) until it is unplugged.
// NULL only when the device table is full.
static struct usb_dev *dev_attach(struct xhci *x, const struct xhci_dev_addr *a, struct usb_dev *parent) {
    char name[16];
    usb_port_name(name, sizeof name, a->root_port, a->route);
    struct usb_dev *u = dev_new();
    if (!u) {
        klog("usb: port %s: device table full, ignored", name);
        return NULL;
    }
    generation++;
    memcpy(u->name, name, sizeof name);
    u->parent = parent;
    u->speed = a->speed;
    const char *sp = usb_speed_name(a->speed);
    struct usb_device_info *di = &u->di;
    struct usb_config_info ci;

    if (!xhci_dev_open(x, &u->xd, a)) {
        klog("usb: port %s: addressing failed", name);
        u->state = USB_DEV_NO_ADDRESS;
        goto ignore;
    }
    if (!xhci_ok(get_desc(&u->xd, 0x0100, 18)) || !usb_desc_device(u->xd.buf, 18, di)) {
        klog("usb: port %s: no device descriptor", name);
        u->state = USB_DEV_NO_DESC;
        goto ignore;
    }
    u->has_id = true;
    u->cls = di->dev_class;
    u->sub = di->sub_class;
    u->proto = di->protocol;
    read_strings(u);
    uint16_t total = 0;
    if (xhci_ok(get_desc(&u->xd, 0x0200, 9)))
        total = (uint16_t)(u->xd.buf[2] | u->xd.buf[3] << 8);
    if (total > USB_CONFIG_MAX)
        total = USB_CONFIG_MAX;
    if (total < 9 || !xhci_ok(get_desc(&u->xd, 0x0200, total)) ||
        !usb_desc_config(u->xd.buf, total, &ci)) {
        klog("usb: port %s %s device %04x:%04x, bad configuration descriptor", name, sp, di->vid, di->pid);
        u->state = USB_DEV_BAD_CONFIG;
        goto ignore;
    }
    if ((u->cls == 0x00 || u->cls == 0xEF) && ci.has_iface) {
        u->cls = ci.iface_class;
        u->sub = ci.iface_sub;
        u->proto = ci.iface_proto;
    }
    if (di->dev_class == 9) {
        if (parent) {
            klog("usb: port %s %s hub behind a hub, ignored", name, sp);
            u->state = USB_DEV_HUB_NESTED;
            goto ignore;
        }
        if (a->speed >= USB_SPEED_SS) {  // its USB 2 twin on a USB 2 port carries the keyboards
            klog("usb: port %s %s hub, ignored", name, sp);
            u->state = USB_DEV_HUB_SS;
            goto ignore;
        }
        if (!hub_setup(u, &ci)) {
            klog("usb: port %s %s hub %04x:%04x, setup failed", name, sp, di->vid, di->pid);
            u->state = USB_DEV_SETUP_FAILED;
            goto ignore;
        }
        return u;
    }
    if (ci.has_kbd || ci.has_mouse) {
        if (!hid_setup(u, &ci)) {
            klog("usb: port %s %s HID %04x:%04x, setup failed", name, sp, di->vid, di->pid);
            u->state = USB_DEV_SETUP_FAILED;
            goto ignore;
        }
        klog("usb: port %s %s %s %04x:%04x", name, sp,
             u->is_kbd && u->is_mouse ? "keyboard+mouse" : u->is_kbd ? "keyboard" : "mouse",
             di->vid, di->pid);
        return u;
    }
    klog("usb: port %s %s device %04x:%04x class %02x, ignored", name, sp, di->vid, di->pid, u->cls);
    u->state = USB_DEV_UNUSED;
ignore:
    xhci_dev_close(&u->xd);
    u->kind = DEV_IGNORED;
    return u;
}

// Brings a root port in line with what is plugged into it. Idempotent: called at
// boot for every port and again for every Port Status Change event.
static void service_root(struct usb_hc *h, uint8_t port, bool debounce) {
    struct xhci *x = &h->x;
    if (!port || port > x->max_ports)
        return;
    uint32_t sc = xhci_port_status(x, port);
    bool conn = sc & XHCI_PORTSC_CCS, replugged = sc & XHCI_PORTSC_CSC;
    struct usb_dev **slot = &h->port_dev[port];
    uint64_t bit = 1ull << (port & 63), *skip = &h->skip[port >> 6];

    if (*slot && (!conn || replugged)) {
        klog("usb: port %s disconnected", (*slot)->name);
        dev_remove(*slot);
        *slot = NULL;
    }
    if (!conn || replugged)
        *skip &= ~bit;
    if (!conn || *slot || (*skip & bit))
        return;
    if (debounce)
        mdelay(100);
    uint8_t speed = 0;
    if (!xhci_port_reset(x, port, &speed)) {
        klog("usb: port %u: reset failed", port);
        *skip |= bit;
        return;
    }
    mdelay(10);  // reset recovery
    struct xhci_dev_addr a = { .root_port = port, .speed = speed };
    *slot = dev_attach(x, &a, NULL);
    if (!*slot)
        *skip |= bit;
}

void usb_init(const struct pci_list *pci) {
    for (size_t i = 0; i < pci->count; i++) {
        const struct pci_device *pd = &pci->devices[i];
        if (pd->class_code != 0x0C || pd->subclass != 0x03 || pd->prog_if != 0x30)
            continue;
        if (hc_count == USB_MAX_HC) {
            klog("usb: more than %d xHCI controllers, %02x:%02x.%x ignored", USB_MAX_HC,
                 pd->bus, pd->dev, pd->fn);
            continue;
        }
        struct usb_hc *h = &hcs[hc_count++];  // kept even when it fails, for the USB tab
        h->vendor = pd->vendor;
        h->up = xhci_init(&h->x, pd);
        hc_up += h->up;
    }
    if (hc_up)
        mdelay(100);  // let freshly powered ports see their devices
    for (size_t i = 0; i < hc_count; i++)
        for (unsigned p = 1; hcs[i].up && p <= hcs[i].x.max_ports; p++)
            service_root(&hcs[i], (uint8_t)p, false);
    klog("usb: %zu xHCI controller%s, %zu keyboard%s", hc_up, hc_up == 1 ? "" : "s",
         kbd_count, kbd_count == 1 ? "" : "s");
}

size_t usb_keyboards(void) {
    return kbd_count;
}

#define KEYQ 32

static struct key_event queue[KEYQ];
static size_t q_head, q_len;

static void push(const struct key_event *e) {
    if (q_len < KEYQ) {
        queue[(q_head + q_len) % KEYQ] = *e;
        q_len++;
    }
}

static bool pop(struct key_event *out) {
    if (!q_len)
        return false;
    *out = queue[q_head];
    q_head = (q_head + 1) % KEYQ;
    q_len--;
    return true;
}

#define PTRQ 8

static struct pointer_event pq[PTRQ];
static size_t pq_head, pq_len;

static int16_t clamp16(int v) {
    return (int16_t)(v < -32768 ? -32768 : v > 32767 ? 32767 : v);
}

// Queues a pointer event, merging its motion into the last one when the buttons
// did not change, so clicks are never lost while motion is coalesced.
static void push_pointer(const struct pointer_event *p) {
    if (pq_len) {
        struct pointer_event *last = &pq[(pq_head + pq_len - 1) % PTRQ];
        if (last->buttons == p->buttons) {
            int w = last->wheel + p->wheel;
            last->dx = clamp16(last->dx + p->dx);
            last->dy = clamp16(last->dy + p->dy);
            last->wheel = (int8_t)(w < -127 ? -127 : w > 127 ? 127 : w);
            return;
        }
    }
    if (pq_len < PTRQ) {
        pq[(pq_head + pq_len) % PTRQ] = *p;
        pq_len++;
    }
}

bool usb_poll_pointer(struct pointer_event *out) {
    if (!pq_len)
        return false;
    *out = pq[pq_head];
    pq_head = (pq_head + 1) % PTRQ;
    pq_len--;
    return true;
}

static void handle_report(struct usb_dev *u, int i) {
    struct xhci_dev *d = &u->xd;
    struct xhci_intr *e = &d->intr[i];
    e->done = false;
    enum xhci_recovery rec = xhci_intr_recovery(e->cc);
    if (rec != XHCI_RECOVER_NONE) {
        if (++u->errors[i] == 1)
            klog("usb: port %s: transfer error (cc %u), recovering", u->name, e->cc);
        if (u->is_kbd && i == u->kbd_ep)
            hid_kbd_cancel_repeat(&u->kbd);  // the lost report may have been a release
        if (u->errors[i] > 8) {
            // Give up on this endpoint only: the other half of a combo receiver keeps working.
            u->stopped[i] = true;
            generation++;
            klog("usb: port %s: too many transfer errors (cc %u), stopped", u->name, e->cc);
            if (u->is_kbd && i == u->kbd_ep) {
                u->is_kbd = false;  // the footer must not claim a keyboard that no longer works
                kbd_count--;
            }
            if (u->is_mouse && i == u->mouse_ep)
                u->is_mouse = false;
            return;
        }
        xhci_ep_reset(d, e->dci, rec == XHCI_RECOVER_SOFT);
        if (rec == XHCI_RECOVER_HALT)  // clear the device's halt too, so both ends restart at DATA0
            xhci_control(d, 0x02, REQ_CLEAR_FEATURE, 0 /* ENDPOINT_HALT */, (uint16_t)(0x80 | (e->dci - 1) / 2), 0);
        xhci_intr_queue(d, i);
        return;
    }
    u->errors[i] = 0;
    const uint8_t *r = xhci_report_buf(d, i);
    if (u->kind == DEV_HUB) {
        // Bitmap: bit 0 = hub itself, bit p = port p changed.
        uint32_t bits = r[0] | (e->actual > 1 ? (uint32_t)r[1] << 8 : 0);
        xhci_intr_queue(d, i);  // bits copied out: the buffer may be refilled
        for (uint8_t p = 1; p <= u->nports; p++)
            if (bits & (1u << p))
                service_hub_port(u, p, true);
    } else if (u->is_kbd && i == u->kbd_ep) {
        struct key_event ev[6];
        size_t n = hid_kbd_report(&u->kbd, r, e->actual, tsc_ms(), ev);
        for (size_t k = 0; k < n; k++)
            push(&ev[k]);
        xhci_intr_queue(d, i);
    } else if (u->is_mouse && i == u->mouse_ep) {
        struct pointer_event p;
        if (hid_mouse_report(r, e->actual, &p))
            push_pointer(&p);
        xhci_intr_queue(d, i);
    } else {
        xhci_intr_queue(d, i);
    }
}

bool usb_poll(struct key_event *out) {
    if (pop(out))
        return true;
    for (size_t i = 0; i < hc_count; i++) {
        struct usb_hc *h = &hcs[i];
        if (!h->up)
            continue;
        xhci_poll(&h->x);
        uint8_t port;
        while (!h->x.dead && xhci_port_take_change(&h->x, &port))
            service_root(h, port, true);
        if (h->x.dead && !h->dead_handled) {
            h->dead_handled = true;
            for (unsigned p = 1; p <= h->x.max_ports; p++) {
                if (h->port_dev[p]) {
                    dev_remove(h->port_dev[p]);
                    h->port_dev[p] = NULL;
                }
            }
        }
    }
    for (size_t i = 0; i < USB_MAX_DEVS; i++) {
        struct usb_dev *u = &devs[i];
        for (int k = 0; k < XHCI_MAX_INTR; k++)
            if (u->used && !u->stopped[k] && u->xd.intr[k].dci && u->xd.intr[k].done)
                handle_report(u, k);
    }
    if (tsc_hz()) {
        uint64_t now = tsc_ms();
        for (size_t i = 0; i < USB_MAX_DEVS; i++) {
            struct key_event e;
            if (devs[i].used && devs[i].is_kbd && hid_kbd_tick(&devs[i].kbd, now, &e))
                push(&e);
        }
    }
    return pop(out);
}

uint32_t usb_generation(void) {
    return generation;
}

static void summarize_dev(struct usb_summary *s, uint8_t hc, const struct usb_dev *u) {
    if (s->dev_count == USB_SUM_DEVS) {
        s->dev_truncated = true;
        return;
    }
    struct usb_dev_summary *d = &s->dev[s->dev_count++];
    memset(d, 0, sizeof *d);
    d->hc = hc;
    memcpy(d->port, u->name, sizeof d->port);
    d->speed = u->speed;
    d->has_id = u->has_id;
    d->vid = u->di.vid;
    d->pid = u->di.pid;
    d->cls = u->cls;
    d->sub = u->sub;
    d->proto = u->proto;
    memcpy(d->manufacturer, u->manufacturer, sizeof d->manufacturer);
    memcpy(d->product, u->product, sizeof d->product);
    switch (u->kind) {
    case DEV_HUB:
        d->state = USB_DEV_HUB;
        d->hub_ports = u->nports;
        break;
    case DEV_HID:
        d->state = u->is_kbd && u->is_mouse ? USB_DEV_KBD_MOUSE : u->is_kbd ? USB_DEV_KEYBOARD
                 : u->is_mouse ? USB_DEV_MOUSE : USB_DEV_STOPPED;
        break;
    default:
        d->state = u->state;
        break;
    }
}

void usb_summarize(struct usb_summary *s) {
    memset(s, 0, sizeof *s);
    for (size_t i = 0; i < hc_count && i < USB_SUM_HCS; i++) {
        const struct usb_hc *h = &hcs[i];
        struct usb_hc_summary *c = &s->hc[s->hc_count++];
        memcpy(c->name, h->x.name, sizeof c->name);
        c->vendor = h->vendor;
        c->version = h->x.version;
        c->ports = h->x.max_ports;
        c->state = !h->up ? USB_HC_FAILED : h->x.dead ? USB_HC_STOPPED : USB_HC_RUNNING;
        for (unsigned p = 1; h->up && p <= h->x.max_ports; p++) {
            c->usb2_ports += h->x.port_major[p] == 2;
            c->usb3_ports += h->x.port_major[p] == 3;
        }
        // Tree order: each root port, then the ports of a hub on it.
        for (unsigned p = 1; h->up && p <= h->x.max_ports; p++) {
            const struct usb_dev *u = h->port_dev[p];
            if (!u)
                continue;
            summarize_dev(s, (uint8_t)i, u);
            for (unsigned k = 1; k <= HUB_MAX_PORTS; k++)
                if (u->child[k])
                    summarize_dev(s, (uint8_t)i, u->child[k]);
        }
    }
}
