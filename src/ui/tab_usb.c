#include "info/pci.h"
#include "lib/fmt.h"
#include "lib/string.h"
#include "ui/tabs.h"
#include "usb/usb_desc.h"
#include "usb/usb_summary.h"

#define ROW_FMT "%-6s  %-6s  %-9s  %-18s  %-17s  %s"

static const char *speed_name(uint8_t speed) {
    static const char *const names[] = { "?", "full", "low", "high", "super", "super+" };
    return speed < sizeof names / sizeof *names ? names[speed] : "?";
}

static const char *state_name(enum usb_dev_state s) {
    switch (s) {
    case USB_DEV_KEYBOARD: return "keyboard";
    case USB_DEV_MOUSE: return "mouse";
    case USB_DEV_KBD_MOUSE: return "keyboard+mouse";
    case USB_DEV_HUB: return "in use";
    case USB_DEV_UNUSED: return "not used";
    case USB_DEV_HUB_SS: return "USB 3 half of hub";
    case USB_DEV_HUB_NESTED: return "hub behind a hub";
    case USB_DEV_STOPPED: return "stopped (errors)";
    case USB_DEV_NO_ADDRESS: return "addressing failed";
    case USB_DEV_NO_DESC: return "no descriptor";
    case USB_DEV_BAD_CONFIG: return "bad config";
    case USB_DEV_SETUP_FAILED: return "setup failed";
    }
    return "?";
}

static bool is_problem(enum usb_dev_state s) {
    return s >= USB_DEV_STOPPED;
}

static void controller(struct page *p, const struct usb_hc_summary *h) {
    char ver[16] = "xHCI", vendor[8], ports[48] = "";
    const char *vname = pci_vendor_name(h->vendor);
    if (!vname) {
        ksnprintf(vendor, sizeof vendor, "%04x", h->vendor);
        vname = vendor;
    }
    if (h->version)
        ksnprintf(ver, sizeof ver, "xHCI %x.%02x", h->version >> 8, h->version & 0xFF);
    if (h->state == USB_HC_FAILED) {
        page_add(p, STYLE_WARN, "%s  %-9s  %s  not started (see the Log tab)", h->name, ver, vname);
        return;
    }
    if (h->usb2_ports || h->usb3_ports)
        ksnprintf(ports, sizeof ports, "%u ports (%u USB 2, %u USB 3)", h->ports, h->usb2_ports,
                  h->usb3_ports);
    else
        ksnprintf(ports, sizeof ports, "%u ports", h->ports);
    if (h->state == USB_HC_STOPPED)
        page_add(p, STYLE_WARN, "%s  %-9s  %s  %s, stopped (controller error)", h->name, ver, vname, ports);
    else
        page_add(p, STYLE_TEXT, "%s  %-9s  %s  %s", h->name, ver, vname, ports);
}

// "Logitech USB Receiver" rather than "Logitech Logitech USB Receiver".
static void product_name(char *buf, size_t cap, const struct usb_dev_summary *d) {
    size_t ml = strlen(d->manufacturer);
    if (!ml || (strlen(d->product) >= ml && memcmp(d->product, d->manufacturer, ml) == 0))
        ksnprintf(buf, cap, "%s", d->product);
    else if (!d->product[0])
        ksnprintf(buf, cap, "%s", d->manufacturer);
    else
        ksnprintf(buf, cap, "%s %s", d->manufacturer, d->product);
}

static void device(struct page *p, const struct usb_dev_summary *d) {
    char id[12] = "-", cls[24] = "-", name[96];
    if (d->has_id) {
        ksnprintf(id, sizeof id, "%04x:%04x", d->vid, d->pid);
        const char *cn = usb_class_name(d->cls, d->sub, d->proto);
        if (d->cls == 9 && d->hub_ports)
            ksnprintf(cls, sizeof cls, "Hub, %u ports", d->hub_ports);
        else if (cn)
            ksnprintf(cls, sizeof cls, "%s", cn);
        else if (d->cls)
            ksnprintf(cls, sizeof cls, "Class %02x", d->cls);
    }
    product_name(name, sizeof name, d);
    page_add(p, is_problem(d->state) ? STYLE_WARN : STYLE_TEXT, ROW_FMT, d->port,
             speed_name(d->speed), id, cls, state_name(d->state), name);
    // Columns are padded; a row without a product name must not end in spaces.
    char *t = p->lines[p->count - 1].text;
    size_t n = strlen(t);
    while (n && t[n - 1] == ' ')
        t[--n] = 0;
}

void tab_usb_build(struct page *p, const struct usb_summary *s) {
    page_clear(p);
    p->title = "USB";
    if (s->disabled) {
        page_add(p, STYLE_WARN, "USB turned off (nousb on the command line)");
        return;
    }
    if (s->no_dma) {
        page_add(p, STYLE_WARN, "USB not started (no DMA memory)");
        return;
    }
    if (!s->hc_count) {
        page_add(p, STYLE_WARN, "No xHCI controllers found (older USB controllers are not supported)");
        return;
    }
    page_add(p, STYLE_HEAD, "Controllers (%zu)", s->hc_count);
    for (size_t i = 0; i < s->hc_count; i++)
        controller(p, &s->hc[i]);

    bool hidden = false;
    for (size_t i = 0; i < s->hc_count; i++) {
        if (s->hc[i].state == USB_HC_FAILED)
            continue;
        size_t n = 0;
        for (size_t k = 0; k < s->dev_count; k++)
            n += s->dev[k].hc == i;
        page_blank(p);
        page_add(p, STYLE_HEAD, "Devices on %s (%zu)", s->hc[i].name, n);
        if (!n) {
            page_add(p, STYLE_DIM, "Nothing attached");
            continue;
        }
        page_add(p, STYLE_DIM, ROW_FMT, "Port", "Speed", "ID", "Class", "Status", "Product");
        for (size_t k = 0; k < s->dev_count; k++) {
            const struct usb_dev_summary *d = &s->dev[k];
            if (d->hc != i)
                continue;
            device(p, d);
            hidden |= d->state == USB_DEV_HUB_SS || d->state == USB_DEV_HUB_NESTED;
        }
    }
    if (hidden) {
        page_blank(p);
        page_add(p, STYLE_DIM, "Devices behind a hub's USB 3 half, or behind a second hub, are not listed.");
    }
    if (s->dev_truncated)
        page_add(p, STYLE_WARN, "More than %d USB devices; list truncated", USB_SUM_DEVS);
}
