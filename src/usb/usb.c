#include "usb/usb.h"
#include "info/tsc.h"
#include "lib/serial.h"
#include "usb/xhci.h"

#define USB_MAX_HC 4

struct usb_hc {
    struct xhci x;
};

static struct usb_hc hcs[USB_MAX_HC];
static size_t hc_count;

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
        if (xhci_init(&hcs[hc_count].x, pd))
            hc_count++;
    }
    klog("usb: %zu xHCI controller%s", hc_count, hc_count == 1 ? "" : "s");
}
