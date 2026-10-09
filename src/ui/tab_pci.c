#include "info/pci.h"
#include "info/pci_db.h"
#include "ui/tabs.h"

void tab_pci_build(struct page *p, const struct pci_list *l) {
    page_clear(p);
    p->title = "PCI";
    if (!l->count) {
        page_add(p, STYLE_WARN, "No PCI devices found");
        return;
    }

    page_add(p, STYLE_HEAD, "Devices (%zu)", l->count);
    page_add(p, STYLE_DIM, "%-7s  %-9s  %-29s  %s", "Address", "ID", "Class", "Vendor and device");
    for (size_t i = 0; i < l->count; i++) {
        const struct pci_device *d = &l->devices[i];
        const char *vendor = pci_vendor_name(d->vendor);
        if (!vendor)
            vendor = pci_db_vendor(&pci_ids, d->vendor);
        const char *device = pci_db_device(&pci_ids, d->vendor, d->device);
        page_add(p, STYLE_TEXT, "%02x:%02x.%x  %04x:%04x  %-29s  %s%s%s", d->bus, d->dev, d->fn,
                 d->vendor, d->device, pci_class_name(d->class_code, d->subclass, d->prog_if),
                 vendor ? vendor : "-", device ? " " : "", device ? device : "");
    }
    if (l->truncated)
        page_add(p, STYLE_WARN, "More than %d PCI functions; list truncated", PCI_MAX_DEVICES);
}
