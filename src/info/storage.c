#include "info/storage.h"
#include "info/pci.h"
#include "lib/fmt.h"
#include "lib/serial.h"

static const char *bus_name(enum disk_bus b) {
    return b == DISK_NVME ? "NVMe" : b == DISK_ATAPI ? "ATAPI" : "SATA";
}

void storage_probe(const struct pci_list *pci, struct storage_list *out) {
    out->probed = true;
    for (size_t i = 0; i < pci->count; i++) {
        const struct pci_device *pd = &pci->devices[i];
        if (pd->class_code != 0x01)
            continue;
        size_t room = STORAGE_MAX_DISKS - out->count;
        if (pd->subclass == 0x08 && pd->prog_if == 0x02) {
            if (out->nvme_ctrls++ >= STORAGE_MAX_CTRLS || !room)
                continue;
            if (nvme_probe(pd, &out->disks[out->count]))
                out->count++;
        } else if (pd->subclass == 0x06 && pd->prog_if == 0x01) {
            if (out->ahci_ctrls++ >= STORAGE_MAX_CTRLS || !room)
                continue;
            out->count += ahci_probe(pd, &out->disks[out->count], room);
        } else if (pd->subclass == 0x04) {
            out->raid_ctrls++;
        } else if (pd->subclass == 0x01) {
            out->ide_ctrls++;
        }
    }
    for (size_t i = 0; i < out->count; i++) {
        const struct disk_info *d = &out->disks[i];
        char size[24] = "no media size";
        if (d->bytes)
            fmt_size(size, sizeof size, d->bytes);
        klog("storage: %s %s, %s, health %s", bus_name(d->bus), d->model, size,
             disk_health_name(disk_health(d)));
    }
    klog("storage: %zu drive%s", out->count, out->count == 1 ? "" : "s");
}
