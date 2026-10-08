#include "test.h"
#include "info/pci.h"

// A fake PCI bus: config reads hit this table, everything else floats (0xFFFFFFFF).
struct fake_fn {
    uint8_t bus, dev, fn;
    uint32_t id;        // offset 0x00: device << 16 | vendor
    uint32_t class_reg; // offset 0x08: class << 24 | subclass << 16 | prog_if << 8 | revision
    uint8_t header;     // header type byte (bit 7 = multi-function)
};

static const struct fake_fn *fake;
static size_t fake_count;

static uint32_t fake_read(uint8_t bus, uint8_t dev, uint8_t fn, uint8_t off) {
    for (size_t i = 0; i < fake_count; i++) {
        const struct fake_fn *f = &fake[i];
        if (f->bus != bus || f->dev != dev || f->fn != fn)
            continue;
        switch (off) {
        case 0x00: return f->id;
        case 0x08: return f->class_reg;
        case 0x0C: return (uint32_t)f->header << 16;
        default: return 0;
        }
    }
    return 0xFFFFFFFF;
}

static struct pci_list list;

static void test_scan(void) {
    static const struct fake_fn bus[] = {
        { 0, 0, 0, 0x29c08086, 0x06000000, 0x00 },  // host bridge
        { 0, 2, 0, 0x11111234, 0x03000002, 0x00 },  // bochs VGA
        { 0, 2, 1, 0xdeadbeef, 0x01080200, 0x00 },  // fn 1 behind a single-function device: must be ignored
        { 0, 31, 0, 0x29188086, 0x06010002, 0x80 }, // multi-function LPC bridge
        { 0, 31, 2, 0x29228086, 0x01060102, 0x00 }, //   fn 2: AHCI
        { 0, 31, 3, 0x29308086, 0x0c050002, 0x00 }, //   fn 3: SMBus
        { 5, 0, 0, 0x5a1d144d, 0x01080201, 0x00 },  // NVMe on bus 5
    };
    fake = bus;
    fake_count = sizeof bus / sizeof bus[0];
    pci_scan(fake_read, &list);

    CHECK_EQ(list.count, 6);
    CHECK(!list.truncated);
    CHECK_EQ(list.devices[0].vendor, 0x8086);
    CHECK_EQ(list.devices[0].device, 0x29c0);
    CHECK_EQ(list.devices[1].dev, 2);
    CHECK_EQ(list.devices[1].revision, 2);
    CHECK_EQ(list.devices[2].dev, 31);
    CHECK_EQ(list.devices[3].fn, 2);
    CHECK_EQ(list.devices[3].class_code, 0x01);
    CHECK_EQ(list.devices[3].subclass, 0x06);
    CHECK_EQ(list.devices[3].prog_if, 0x01);
    CHECK_EQ(list.devices[4].fn, 3);
    CHECK_EQ(list.devices[5].bus, 5);
    CHECK_EQ(list.devices[5].device, 0x5a1d);
}

static void test_empty_and_truncated(void) {
    fake = NULL;
    fake_count = 0;
    pci_scan(fake_read, &list);
    CHECK_EQ(list.count, 0);
    CHECK(!list.truncated);

    static struct fake_fn many[200];
    for (int i = 0; i < 200; i++)
        many[i] = (struct fake_fn){ (uint8_t)(i / 32), (uint8_t)(i % 32), 0, 0x00011af4, 0x02000000, 0 };
    fake = many;
    fake_count = 200;
    pci_scan(fake_read, &list);
    CHECK_EQ(list.count, PCI_MAX_DEVICES);
    CHECK(list.truncated);
}

static void test_names(void) {
    CHECK_STR(pci_class_name(0x01, 0x08, 0x02), "NVMe controller");
    CHECK_STR(pci_class_name(0x01, 0x06, 0x01), "SATA controller (AHCI)");
    CHECK_STR(pci_class_name(0x0c, 0x03, 0x30), "USB xHCI controller");
    CHECK_STR(pci_class_name(0x0c, 0x03, 0xfe), "USB controller");      // prog-if wildcard
    CHECK_STR(pci_class_name(0x03, 0x00, 0x00), "VGA-compatible controller");
    CHECK_STR(pci_class_name(0x02, 0x42, 0x00), "Network controller");  // class-level fallback
    CHECK_STR(pci_class_name(0x42, 0x00, 0x00), "Unknown class");
    CHECK_STR(pci_class_name(0xff, 0x00, 0x00), "Unknown class");

    CHECK_STR(pci_vendor_name(0x8086), "Intel");
    CHECK_STR(pci_vendor_name(0x1022), "AMD");
    CHECK_STR(pci_vendor_name(0x10de), "NVIDIA");
    CHECK_STR(pci_vendor_name(0x1234), "QEMU");
    CHECK(pci_vendor_name(0x4242) == NULL);
}

int main(void) {
    test_scan();
    test_empty_and_truncated();
    test_names();
    // BAR helpers.
    CHECK_EQ(pci_bar_base(0xFEBF0000, 0), 0xFEBF0000);
    CHECK_EQ(pci_bar_base(0xFEBF0008, 0), 0xFEBF0000);               // prefetchable flag masked
    CHECK_EQ(pci_bar_base(0xFEB00004, 0x1), 0x1FEB00000ull);         // 64-bit BAR
    CHECK_EQ(pci_bar_base(0xFEB00000, 0x1), 0xFEB00000);             // 32-bit: hi ignored
    CHECK_EQ(pci_bar_base(0xC001, 0), 0);                            // I/O BAR
    CHECK_EQ(pci_bar_size(0xFFFF0000), 0x10000);
    CHECK_EQ(pci_bar_size(0xFFFF0004), 0x10000);
    CHECK_EQ(pci_bar_size(0xFFFFC000), 0x4000);
    CHECK_EQ(pci_bar_size(0), 0);
    CHECK_EQ(pci_bar_size(0xFFFFFF01), 0);                           // I/O
    TEST_MAIN_END();
}
