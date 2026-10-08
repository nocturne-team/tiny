#include "info/pci.h"
#include "lib/string.h"

#define ANY 0x100  // matches any subclass / prog-if

static void add(struct pci_list *out, pci_read_fn read, uint8_t bus, uint8_t dev, uint8_t fn, uint32_t id) {
    if (out->count == PCI_MAX_DEVICES) {
        out->truncated = true;
        return;
    }
    uint32_t cls = read(bus, dev, fn, 0x08);
    struct pci_device *d = &out->devices[out->count++];
    d->bus = bus;
    d->dev = dev;
    d->fn = fn;
    d->vendor = (uint16_t)id;
    d->device = (uint16_t)(id >> 16);
    d->revision = (uint8_t)cls;
    d->prog_if = (uint8_t)(cls >> 8);
    d->subclass = (uint8_t)(cls >> 16);
    d->class_code = (uint8_t)(cls >> 24);
    d->header_type = (uint8_t)((read(bus, dev, fn, 0x0C) >> 16) & 0x7F);
}

void pci_scan(pci_read_fn read, struct pci_list *out) {
    memset(out, 0, sizeof *out);
    for (unsigned bus = 0; bus < 256; bus++) {
        for (uint8_t dev = 0; dev < 32; dev++) {
            uint32_t id = read((uint8_t)bus, dev, 0, 0x00);
            if ((id & 0xFFFF) == 0xFFFF)
                continue;
            add(out, read, (uint8_t)bus, dev, 0, id);
            uint8_t header = (uint8_t)(read((uint8_t)bus, dev, 0, 0x0C) >> 16);
            if (!(header & 0x80))
                continue;
            for (uint8_t fn = 1; fn < 8; fn++) {
                id = read((uint8_t)bus, dev, fn, 0x00);
                if ((id & 0xFFFF) != 0xFFFF)
                    add(out, read, (uint8_t)bus, dev, fn, id);
            }
            if (out->truncated)
                return;
        }
    }
}

static const struct {
    uint8_t class_code;
    uint16_t subclass, prog_if;
    const char *name;
} class_table[] = {
    { 0x01, 0x00, ANY, "SCSI controller" },
    { 0x01, 0x01, ANY, "IDE controller" },
    { 0x01, 0x04, ANY, "RAID controller" },
    { 0x01, 0x05, ANY, "ATA controller" },
    { 0x01, 0x06, 0x01, "SATA controller (AHCI)" },
    { 0x01, 0x06, ANY, "SATA controller" },
    { 0x01, 0x07, ANY, "SAS controller" },
    { 0x01, 0x08, 0x02, "NVMe controller" },
    { 0x01, 0x08, ANY, "NVM controller" },
    { 0x02, 0x00, ANY, "Ethernet controller" },
    { 0x03, 0x00, ANY, "VGA-compatible controller" },
    { 0x03, 0x01, ANY, "XGA controller" },
    { 0x03, 0x02, ANY, "3D controller" },
    { 0x04, 0x01, ANY, "Audio device" },
    { 0x04, 0x03, ANY, "HD Audio controller" },
    { 0x06, 0x00, ANY, "Host bridge" },
    { 0x06, 0x01, ANY, "ISA bridge" },
    { 0x06, 0x04, ANY, "PCI-to-PCI bridge" },
    { 0x06, 0x09, ANY, "PCI-to-PCI bridge" },
    { 0x07, 0x00, ANY, "Serial controller" },
    { 0x08, 0x00, ANY, "Interrupt controller" },
    { 0x08, 0x05, ANY, "SD host controller" },
    { 0x08, 0x06, ANY, "IOMMU" },
    { 0x09, 0x00, ANY, "Keyboard controller" },
    { 0x0C, 0x00, ANY, "FireWire controller" },
    { 0x0C, 0x03, 0x00, "USB UHCI controller" },
    { 0x0C, 0x03, 0x10, "USB OHCI controller" },
    { 0x0C, 0x03, 0x20, "USB EHCI controller" },
    { 0x0C, 0x03, 0x30, "USB xHCI controller" },
    { 0x0C, 0x03, 0x40, "USB4 host controller" },
    { 0x0C, 0x03, ANY, "USB controller" },
    { 0x0C, 0x05, ANY, "SMBus controller" },
    { 0x0C, 0x80, ANY, "Serial bus controller" },
    { 0x0D, 0x11, ANY, "Bluetooth controller" },
    { 0x0D, 0x80, ANY, "Wireless controller" },
};

static const char *const class_names[] = {
    [0x00] = "Unclassified device",
    [0x01] = "Mass storage controller",
    [0x02] = "Network controller",
    [0x03] = "Display controller",
    [0x04] = "Multimedia controller",
    [0x05] = "Memory controller",
    [0x06] = "Bridge",
    [0x07] = "Communication controller",
    [0x08] = "System peripheral",
    [0x09] = "Input device controller",
    [0x0A] = "Docking station",
    [0x0B] = "Processor",
    [0x0C] = "Serial bus controller",
    [0x0D] = "Wireless controller",
    [0x0E] = "Intelligent controller",
    [0x0F] = "Satellite controller",
    [0x10] = "Encryption controller",
    [0x11] = "Signal processing controller",
    [0x12] = "Processing accelerator",
    [0x13] = "Non-essential instrumentation",
};

const char *pci_class_name(uint8_t class_code, uint8_t subclass, uint8_t prog_if) {
    for (size_t i = 0; i < sizeof class_table / sizeof class_table[0]; i++) {
        if (class_table[i].class_code != class_code)
            continue;
        if (class_table[i].subclass != ANY && class_table[i].subclass != subclass)
            continue;
        if (class_table[i].prog_if != ANY && class_table[i].prog_if != prog_if)
            continue;
        return class_table[i].name;
    }
    if (class_code < sizeof class_names / sizeof class_names[0] && class_names[class_code])
        return class_names[class_code];
    return "Unknown class";
}

static const struct {
    uint16_t id;
    const char *name;
} vendor_table[] = {
    { 0x1002, "AMD/ATI" },   { 0x1022, "AMD" },        { 0x1028, "Dell" },
    { 0x102B, "Matrox" },    { 0x103C, "HP" },         { 0x106B, "Apple" },
    { 0x10DE, "NVIDIA" },    { 0x10EC, "Realtek" },    { 0x1106, "VIA" },
    { 0x1179, "Toshiba" },   { 0x11AB, "Marvell" },    { 0x1217, "O2 Micro" },
    { 0x1234, "QEMU" },      { 0x126F, "Silicon Motion" }, { 0x1344, "Micron" },
    { 0x144D, "Samsung" },   { 0x14E4, "Broadcom" },   { 0x15AD, "VMware" },
    { 0x15B7, "SanDisk/WD" }, { 0x168C, "Qualcomm Atheros" }, { 0x1912, "Renesas" },
    { 0x1987, "Phison" },    { 0x1AF4, "Red Hat (virtio)" }, { 0x1B21, "ASMedia" },
    { 0x1B36, "Red Hat" },   { 0x1B4B, "Marvell" },    { 0x1C5C, "SK hynix" },
    { 0x1D6A, "Aquantia" },  { 0x1E0F, "KIOXIA" },     { 0x8086, "Intel" },
    { 0x80EE, "VirtualBox" },
};

const char *pci_vendor_name(uint16_t vendor) {
    for (size_t i = 0; i < sizeof vendor_table / sizeof vendor_table[0]; i++)
        if (vendor_table[i].id == vendor)
            return vendor_table[i].name;
    return NULL;
}

uint64_t pci_bar_base(uint32_t lo, uint32_t hi) {
    if (lo & 1)
        return 0;
    uint64_t base = lo & ~0xFu;
    if (((lo >> 1) & 3) == 2)
        base |= (uint64_t)hi << 32;
    return base;
}

uint64_t pci_bar_size(uint32_t probe) {
    if (probe & 1)
        return 0;
    uint32_t mask = probe & ~0xFu;
    return mask ? (uint32_t)(~mask + 1u) : 0;
}
