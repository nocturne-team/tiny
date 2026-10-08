#include "test.h"
#include "info/cpu.h"
#include "info/fw.h"
#include "info/pci.h"
#include "ui/about_art.h"
#include "ui/tabs.h"

static struct page p;

static const char *find_line(const struct page *pg, const char *prefix) {
    for (size_t i = 0; i < pg->count; i++)
        if (strncmp(pg->lines[i].text, prefix, strlen(prefix)) == 0)
            return pg->lines[i].text;
    return "";
}

static int count_style(const struct page *pg, int style) {
    int n = 0;
    for (size_t i = 0; i < pg->count; i++)
        n += pg->lines[i].style == style;
    return n;
}

static void test_cpu(void) {
    struct cpu_info c;
    memset(&c, 0, sizeof c);
    strcpy(c.vendor, "GenuineIntel");
    strcpy(c.brand, "Test CPU @ 3.70GHz");
    c.sig = (struct cpu_sig){ 6, 158, 10 };
    c.cpu_count = 4;
    static const char *feats[] = { "sse", "sse2", "avx" };
    for (int i = 0; i < 3; i++)
        c.features[i] = feats[i];
    c.feature_count = 3;

    tab_cpu_build(&p, &c, 76);
    CHECK_STR(p.title, "CPU");
    CHECK_STR(find_line(&p, "Vendor") + 16, "GenuineIntel");
    CHECK_STR(find_line(&p, "Signature") + 16, "family 6, model 158 (0x9e), stepping 10");
    CHECK_STR(find_line(&p, "TSC") + 16, "unknown");
    CHECK_STR(find_line(&p, "Not reported"), "Not reported by CPUID");
    CHECK_STR(find_line(&p, "                sse") + 16, "sse sse2 avx");

    c.tsc_hz = 3696000000ULL;
    c.tsc_source = TSC_SRC_PIT;
    tab_cpu_build(&p, &c, 76);
    CHECK_STR(find_line(&p, "TSC") + 16, "~3696.000 MHz (PIT calibrated)");
}

static void test_mem(void) {
    struct limine_memmap_entry e[2] = {
        { 0x0, 0x9fc00, LIMINE_MEMMAP_USABLE },
        { 0x100000, 0x1ff00000, LIMINE_MEMMAP_USABLE },
    };
    struct limine_memmap_entry *ptrs[2] = { &e[0], &e[1] };
    struct limine_memmap_response map = { .revision = 0, .entry_count = 2, .entries = ptrs };

    tab_mem_build(&p, &map);
    CHECK_STR(p.title, "Memory");
    CHECK_STR(find_line(&p, "Available") + 16, "511.6 MiB");
    CHECK_STR(find_line(&p, "0000000000000000"),
              "0000000000000000  000000000009fbff   639.0 KiB  usable");

    tab_mem_build(&p, NULL);
    CHECK_STR(p.lines[0].text, "Memory map not provided by bootloader");
}

static void test_mem_huge(void) {  // Review Focus #3
    static struct limine_memmap_entry e[400];
    static struct limine_memmap_entry *ptrs[400];
    for (int i = 0; i < 400; i++) {
        e[i] = (struct limine_memmap_entry){ (uint64_t)i << 20, 1 << 20, LIMINE_MEMMAP_USABLE };
        ptrs[i] = &e[i];
    }
    struct limine_memmap_response map = { .revision = 0, .entry_count = 400, .entries = ptrs };
    tab_mem_build(&p, &map);
    CHECK_EQ(p.count, PAGE_MAX_LINES);
    CHECK(p.truncated);
    CHECK(strncmp(p.lines[PAGE_MAX_LINES - 1].text, "...", 3) == 0);
}

static void test_fw(void) {  // Review Focus #5
    static struct fw_info f;
    memset(&f, 0, sizeof f);
    f.firmware_type = -1;
    tab_fw_build(&p, &f);
    CHECK_STR(p.title, "Firmware");
    CHECK_STR(find_line(&p, "Firmware") + 16, "unknown");
    CHECK_EQ(count_style(&p, STYLE_WARN), 2);  // SMBIOS and ACPI both missing

    f.firmware_type = LIMINE_FIRMWARE_TYPE_EFI64;
    f.acpi.present = true;
    f.acpi.revision = 2;
    strcpy(f.acpi.oem_id, "BOCHS");
    f.acpi.use_xsdt = true;
    f.acpi.table_count = 1;
    strcpy(f.acpi.tables[0].sig, "APIC");
    strcpy(f.acpi.tables[0].oem_id, "BOCHS");
    f.acpi.tables[0].length = 120;
    f.acpi.tables[0].checksum_ok = false;
    tab_fw_build(&p, &f);
    CHECK_STR(find_line(&p, "Firmware") + 16, "UEFI (64-bit)");
    CHECK_STR(find_line(&p, "  APIC"), "  APIC  BOCHS        120  (bad csum)");
}

static void test_pci(void) {
    static struct pci_list l;
    memset(&l, 0, sizeof l);
    tab_pci_build(&p, &l);
    CHECK_STR(p.title, "PCI");
    CHECK_STR(find_line(&p, "No PCI"), "No PCI devices found");

    l.count = 2;
    l.devices[0] = (struct pci_device){ 0, 31, 2, 0x8086, 0x2922, 0x01, 0x06, 0x01, 0x02, 0 };
    l.devices[1] = (struct pci_device){ 5, 0, 0, 0x4242, 0x0001, 0x42, 0x00, 0x00, 0x00, 0 };
    tab_pci_build(&p, &l);
    CHECK_STR(find_line(&p, "00:1f.2"), "00:1f.2  8086:2922  SATA controller (AHCI)         Intel");
    CHECK_STR(find_line(&p, "05:00.0"), "05:00.0  4242:0001  Unknown class                  -");
    CHECK_STR(find_line(&p, "Devices"), "Devices (2)");

    l.truncated = true;
    tab_pci_build(&p, &l);
    CHECK_EQ(p.lines[p.count - 1].style, STYLE_WARN);
}

static void test_about(void) {
    tab_about_build(&p);
    CHECK_STR(p.title, "About");
    for (int i = 0; i < ABOUT_ART_ROWS; i++) {
        CHECK_EQ(p.lines[i].style, STYLE_ART);
        CHECK_EQ((unsigned char)p.lines[i].text[0], i + 1);  // art row, 1-based
    }
    CHECK(strncmp(find_line(&p, "tiny v"), "tiny v0.2", 9) == 0);
    CHECK(strstr(find_line(&p, "Bootloader"), "Limine") != NULL);
    CHECK(!p.truncated);
}

int main(void) {
    test_about();
    test_pci();
    test_cpu();
    test_mem();
    test_mem_huge();
    test_fw();
    TEST_MAIN_END();
}
