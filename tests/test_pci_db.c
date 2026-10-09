#include "test.h"
#include "info/pci_db.h"

static const char strings[] = "First\0Intel\0SATA\0DRAM\0Last\0Last dev\0";
static const struct pci_db_device devices[] = {
    { 0x0001, 0 },                       // First's only device: "First"
    { 0x2922, 12 }, { 0x29C0, 17 },      // Intel
    { 0xFFFF, 27 },                      // Last
};
static const struct pci_db_vendor vendors[] = {
    { 0x0001, 1, 0, 0 },
    { 0x8086, 2, 6, 1 },
    { 0x9000, 0, 6, 3 },                 // a vendor with no devices
    { 0xFFFF, 1, 22, 3 },
};
static const struct pci_db fixture = { strings, vendors, 4, devices, 4, "test" };

#define NONNULL(s) ((s) ? (s) : "(null)")

int main(void) {
    CHECK_STR(NONNULL(pci_db_vendor(&fixture, 0x0001)), "First");
    CHECK_STR(NONNULL(pci_db_vendor(&fixture, 0x8086)), "Intel");
    CHECK_STR(NONNULL(pci_db_vendor(&fixture, 0xFFFF)), "Last");
    CHECK(pci_db_vendor(&fixture, 0x0000) == NULL);
    CHECK(pci_db_vendor(&fixture, 0x8087) == NULL);
    CHECK_STR(NONNULL(pci_db_device(&fixture, 0x8086, 0x2922)), "SATA");
    CHECK_STR(NONNULL(pci_db_device(&fixture, 0x8086, 0x29C0)), "DRAM");
    CHECK_STR(NONNULL(pci_db_device(&fixture, 0x0001, 0x0001)), "First");
    CHECK_STR(NONNULL(pci_db_device(&fixture, 0xFFFF, 0xFFFF)), "Last dev");
    CHECK(pci_db_device(&fixture, 0x8086, 0x0001) == NULL);  // another vendor's device ID
    CHECK(pci_db_device(&fixture, 0x8086, 0xFFFF) == NULL);
    CHECK(pci_db_device(&fixture, 0x9000, 0x0001) == NULL);  // vendor without devices
    CHECK(pci_db_device(&fixture, 0x4242, 0x2922) == NULL);  // unknown vendor

    static const struct pci_db empty = { "", NULL, 0, NULL, 0, "" };
    CHECK(pci_db_vendor(&empty, 0x8086) == NULL);
    CHECK(pci_db_device(&empty, 0x8086, 0x2922) == NULL);

    // The generated table: sorted, in range, and the names QEMU's devices need.
    const struct pci_db *db = &pci_ids;
    CHECK(db->nvendors > 2000);
    CHECK(db->ndevices > 15000);
    size_t bad = 0;
    for (size_t i = 0; i < db->nvendors; i++) {
        const struct pci_db_vendor *v = &db->vendors[i];
        if (i && v->id <= db->vendors[i - 1].id)
            bad++;
        if ((size_t)v->first + v->count > db->ndevices)
            bad++;
        for (uint32_t j = 1; j < v->count; j++)
            if (db->devices[v->first + j].id <= db->devices[v->first + j - 1].id)
                bad++;
    }
    CHECK_EQ(bad, 0);
    CHECK_STR(NONNULL(pci_db_vendor(db, 0x8086)), "Intel Corporation");
    CHECK_STR(NONNULL(pci_db_device(db, 0x1B36, 0x000D)), "QEMU XHCI Host Controller");
    CHECK_STR(NONNULL(pci_db_device(db, 0x8086, 0x2922)),
              "82801IR/IO/IH (ICH9R/DO/DH) 6 port SATA Controller [AHCI mode]");
    CHECK(db->version[0] != 0);
    TEST_MAIN_END();
}
