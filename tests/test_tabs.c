#include "test.h"
#include "info/cpu.h"
#include "info/fw.h"
#include "info/pci.h"
#include "info/disk.h"
#include "info/sensors.h"
#include "lib/logbuf.h"
#include "ui/about_art.h"
#include "ui/tabs.h"
#include "usb/usb_summary.h"
#include "usb/xhci_calc.h"

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
    c.tsc_source = TSC_SRC_CPUID_BASE;
    tab_cpu_build(&p, &c, 76);
    CHECK_STR(find_line(&p, "TSC") + 16, "~3696.000 MHz (CPUID base clock)");
}

static void test_mem(void) {
    struct limine_memmap_entry e[2] = {
        { 0x0, 0x9fc00, LIMINE_MEMMAP_USABLE },
        { 0x100000, 0x1ff00000, LIMINE_MEMMAP_USABLE },
    };
    struct limine_memmap_entry *ptrs[2] = { &e[0], &e[1] };
    struct limine_memmap_response map = { .revision = 0, .entry_count = 2, .entries = ptrs };

    tab_mem_build(&p, &map, NULL);
    CHECK_STR(p.title, "Memory");
    CHECK_STR(find_line(&p, "Available") + 16, "511.6 MiB");
    CHECK_STR(find_line(&p, "0000000000000000"),
              "0000000000000000  000000000009fbff   639.0 KiB  usable");

    tab_mem_build(&p, NULL, NULL);
    CHECK_STR(p.lines[0].text, "Memory map not provided by bootloader");
}

static void test_mem_dimms(void) {
    struct limine_memmap_entry e = { 0x100000, 0x1ff00000, LIMINE_MEMMAP_USABLE };
    struct limine_memmap_entry *ptrs[1] = { &e };
    struct limine_memmap_response map = { .revision = 0, .entry_count = 1, .entries = ptrs };
    static struct smbios_info smb;
    memset(&smb, 0, sizeof smb);
    smb.present = true;
    smb.dimm_count = 2;
    strcpy(smb.dimms[0].locator, "DIMM_A1");
    smb.dimms[0].size = 8ULL << 30;
    smb.dimms[0].type = 0x1A;
    smb.dimms[0].speed_mts = 3200;
    strcpy(smb.dimms[0].manufacturer, "Kingston");
    strcpy(smb.dimms[0].part, "KF432C16BB/8");
    strcpy(smb.dimms[1].locator, "DIMM_A2");

    tab_mem_build(&p, &map, &smb);
    CHECK_STR(find_line(&p, "Installed"), "Installed modules (1 of 2 slots)");
    CHECK_STR(find_line(&p, "Total installed") + 16, "8.0 GiB");
    CHECK_STR(find_line(&p, "DIMM_A1"), "DIMM_A1           8.0 GiB  DDR4    3200 MT/s  Kingston KF432C16BB/8");
    CHECK_STR(find_line(&p, "DIMM_A2"), "DIMM_A2             empty");

    smb.dimm_count = 0;  // SMBIOS without type 17: no section at all
    tab_mem_build(&p, &map, &smb);
    CHECK_STR(find_line(&p, "Installed"), "");
}

static void test_mem_huge(void) {  // Review Focus #3
    static struct limine_memmap_entry e[400];
    static struct limine_memmap_entry *ptrs[400];
    for (int i = 0; i < 400; i++) {
        e[i] = (struct limine_memmap_entry){ (uint64_t)i << 20, 1 << 20, LIMINE_MEMMAP_USABLE };
        ptrs[i] = &e[i];
    }
    struct limine_memmap_response map = { .revision = 0, .entry_count = 400, .entries = ptrs };
    tab_mem_build(&p, &map, NULL);
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
    CHECK_EQ(count_style(&p, STYLE_WARN), 3);  // EDID, SMBIOS and ACPI all missing
    f.acpi.rsdp_invalid = true;
    tab_fw_build(&p, &f);
    CHECK_STR(find_line(&p, "RSDP"), "RSDP from bootloader has a bad signature");
    f.acpi.rsdp_invalid = false;

    f.smbios.present = true;  // empty strings collapse to a single dash
    tab_fw_build(&p, &f);
    CHECK_STR(find_line(&p, "Board") + 16, "-");
    strcpy(f.smbios.board_vendor, "ASUSTeK");
    tab_fw_build(&p, &f);
    CHECK_STR(find_line(&p, "Board") + 16, "ASUSTeK");
    f.smbios.present = false;

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
    f.acpi.tables[0].mapped = true;
    tab_fw_build(&p, &f);
    CHECK_STR(find_line(&p, "Firmware") + 16, "UEFI (64-bit)");
    CHECK_STR(find_line(&p, "  APIC"), "  APIC  BOCHS        120  (bad csum)");

    f.acpi.table_count = 2;
    strcpy(f.acpi.tables[1].sig, "????");
    tab_fw_build(&p, &f);
    CHECK_STR(find_line(&p, "  ????"), "  ????  -              -  (unmapped)");
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

static void test_fw_monitor(void) {
    static struct fw_info f;
    memset(&f, 0, sizeof f);
    f.firmware_type = -1;
    tab_fw_build(&p, &f);
    CHECK_STR(find_line(&p, "Monitor"), "Monitor");
    CHECK_STR(find_line(&p, "No EDID"), "No EDID from the firmware");

    f.have_edid = true;  // bytes present but not an EDID
    tab_fw_build(&p, &f);
    CHECK_STR(find_line(&p, "EDID present"), "EDID present but invalid");

    f.edid = (struct edid_info){ .valid = true, .checksum_ok = true, .manufacturer = "DEL",
        .product = 0xA0C2, .name = "DELL U2720Q", .week = 12, .year = 2020, .version = 1,
        .revision = 4, .width_cm = 53, .height_cm = 30, .diagonal_tenths = 240,
        .pref_width = 1920, .pref_height = 1080, .pref_centihz = 5994 };
    tab_fw_build(&p, &f);
    CHECK_STR(find_line(&p, "Name") + 16, "DELL U2720Q");
    CHECK_STR(find_line(&p, "Manufacturer") + 16, "DEL, product 0xa0c2");
    CHECK_STR(find_line(&p, "Manufactured") + 16, "week 12 of 2020");
    CHECK_STR(find_line(&p, "Size") + 16, "53 x 30 cm (24.0\")");
    CHECK_STR(find_line(&p, "Preferred mode") + 16, "1920x1080 @ 59.94 Hz");
    CHECK_STR(find_line(&p, "EDID") + 16, "version 1.4");

    f.edid.model_year = true;
    f.edid.checksum_ok = false;
    tab_fw_build(&p, &f);
    CHECK_STR(find_line(&p, "Manufactured") + 16, "model year 2020");
    CHECK_STR(find_line(&p, "EDID") + 16, "version 1.4 (bad csum)");
}

static void test_log(void) {
    static struct logbuf lb;
    memset(&lb, 0, sizeof lb);
    tab_log_build(&p, &lb);
    CHECK_STR(p.title, "Log");
    CHECK_STR(find_line(&p, "Kernel log"), "Kernel log (0 lines)");

    logbuf_add(&lb, 0, "tiny: booting");
    logbuf_add(&lb, 12345, "usb: port 5 HS keyboard 0627:0001");
    logbuf_add(&lb, 99999999, "xhci: 00:04.0 command timeout");
    tab_log_build(&p, &lb);
    CHECK_STR(find_line(&p, "Kernel log"), "Kernel log (3 lines)");
    CHECK_STR(find_line(&p, "    0.000"), "    0.000  tiny: booting");
    CHECK_STR(find_line(&p, "   12.345"), "   12.345  usb: port 5 HS keyboard 0627:0001");
    CHECK_STR(find_line(&p, "99999.999"), "99999.999  xhci: 00:04.0 command timeout");
    CHECK_EQ(count_style(&p, STYLE_WARN), 1);  // the timeout line stands out
    CHECK_EQ(p.lines[p.count - 1].style, STYLE_WARN);

    for (int i = 0; i < LOGBUF_LINES; i++)
        logbuf_add(&lb, 1, "filler");
    tab_log_build(&p, &lb);
    CHECK_STR(find_line(&p, "Kernel log"), "Kernel log (256 lines, 3 older dropped)");
    CHECK(!p.truncated);
}

static int count_prefix(const struct page *pg, const char *prefix) {
    int n = 0;
    for (size_t i = 0; i < pg->count; i++)
        n += strncmp(pg->lines[i].text, prefix, strlen(prefix)) == 0;
    return n;
}

static void test_storage(void) {
    static struct storage_list s;
    memset(&s, 0, sizeof s);
    tab_storage_build(&p, &s);
    CHECK_STR(p.title, "Storage");
    CHECK_STR(find_line(&p, "Storage"), "Storage not probed (no DMA memory)");
    s.disabled = true;
    tab_storage_build(&p, &s);
    CHECK_STR(find_line(&p, "Storage"), "Storage probing turned off (nostorage on the command line)");
    s.disabled = false;

    s.probed = true;
    tab_storage_build(&p, &s);
    CHECK_STR(find_line(&p, "No drives"), "No drives found");

    s.raid_ctrls = 1;
    s.ide_ctrls = 2;
    tab_storage_build(&p, &s);
    CHECK_STR(find_line(&p, "1 RAID"), "1 RAID/VMD controller: drives behind it are not shown.");
    CHECK_STR(find_line(&p, "Switch"), "Switch the firmware's storage mode to AHCI to see them.");
    CHECK_STR(find_line(&p, "2 legacy"), "2 legacy IDE controllers: not supported");
    s.raid_ctrls = s.ide_ctrls = 0;

    struct disk_info *n = &s.disks[0];
    disk_init(n, DISK_NVME);
    n->pci_bus = 2;
    strcpy(n->model, "Samsung SSD 980 PRO 1TB");
    strcpy(n->serial, "S5GX");
    strcpy(n->firmware, "5B2QGXA7");
    n->namespaces = 1;
    n->bytes = 1000204886016ull;
    n->sector = 512;
    n->smart = true;
    n->has_temp = true;
    n->temp_c = 38;
    n->spare = 100;
    n->spare_thresh = 10;
    n->used_pct = 3;
    n->bytes_read = 12345000000000ull;
    n->bytes_written = 512000;
    n->power_on_hours = 1234;
    n->power_cycles = 567;
    n->unsafe_shutdowns = 42;
    n->media_errors = 0;
    n->error_entries = 7;

    struct disk_info *a = &s.disks[1];
    disk_init(a, DISK_SATA);
    a->pci_dev = 0x1f;
    a->pci_fn = 2;
    a->port = 1;
    strcpy(a->model, "QEMU HARDDISK");
    a->bytes = 10737418240ull;
    a->sector = 512;
    a->rpm = 7200;
    a->smart_supported = a->smart_enabled = a->smart = true;
    a->realloc = 8;
    a->pending = 0;
    a->power_on_hours = 10;

    struct disk_info *c = &s.disks[2];
    disk_init(c, DISK_ATAPI);
    c->port = 2;
    strcpy(c->model, "QEMU DVD-ROM");

    struct disk_info *o = &s.disks[3];
    disk_init(o, DISK_SATA);
    strcpy(o->model, "Old");
    o->rpm = 1;
    o->smart_supported = true;
    s.count = 4;
    tab_storage_build(&p, &s);

    CHECK_STR(find_line(&p, "Drives"), "Drives (4)");
    CHECK_STR(find_line(&p, "NVMe "), "NVMe  Samsung SSD 980 PRO 1TB");
    CHECK_STR(find_line(&p, "Interface"), "Interface       NVMe at 02:00.0, 1 namespace");
    CHECK_STR(find_line(&p, "Capacity"), "Capacity        1.0 TB (512-byte sectors)");
    CHECK_STR(find_line(&p, "Serial"), "Serial          S5GX");
    CHECK_STR(find_line(&p, "Health"), "Health          OK");
    CHECK_STR(find_line(&p, "Temperature"), "Temperature     38 \xf8""C");
    CHECK_STR(find_line(&p, "Power-on"), "Power-on        1234 hours, 567 power cycles");
    CHECK_STR(find_line(&p, "Life used"), "Life used       3% (spare 100%, threshold 10%)");
    CHECK_STR(find_line(&p, "Data"), "Data            12.3 TB read, 512.0 kB written");
    CHECK_STR(find_line(&p, "Unsafe"), "Unsafe shutdown 42");
    CHECK_STR(find_line(&p, "Media errors"), "Media errors    0");
    CHECK_STR(find_line(&p, "Error log"), "Error log       7 entries");

    CHECK_STR(find_line(&p, "SATA "), "SATA  QEMU HARDDISK");
    CHECK_STR(find_line(&p, "Interface       SATA"), "Interface       SATA (AHCI 00:1f.2, port 1), 7200 rpm");
    CHECK_STR(find_line(&p, "Capacity        10"), "Capacity        10.7 GB (512-byte sectors)");
    CHECK_STR(find_line(&p, "Health          w"), "Health          warning (8 reallocated, 0 pending, 0 uncorrectable sectors)");
    CHECK_STR(find_line(&p, "Reallocated"), "Reallocated     8 sectors");
    CHECK_STR(find_line(&p, "Power-on        10"), "Power-on        10 hours");
    CHECK_EQ(count_prefix(&p, "Uncorrectable"), 0);  // not reported by the drive

    CHECK_STR(find_line(&p, "Optical"), "Optical  QEMU DVD-ROM");
    CHECK_STR(find_line(&p, "Interface       SATA (AHCI 00:00.0, port 2)"), "Interface       SATA (AHCI 00:00.0, port 2)");

    CHECK_STR(find_line(&p, "Interface       SATA (AHCI 00:00.0, port 0)"), "Interface       SATA (AHCI 00:00.0, port 0), SSD");
    CHECK_STR(find_line(&p, "SMART"), "SMART           supported but disabled");
    CHECK_EQ(count_prefix(&p, "Capacity"), 2);  // unknown capacity is left out
    CHECK_EQ(count_style(&p, STYLE_HEAD), 5);   // "Drives" + one per drive
    CHECK_EQ(count_style(&p, STYLE_WARN), 1);   // the SATA warning

    n->error_entries = 1;
    n->power_on_hours = 1;
    n->power_cycles = 1;
    a->realloc = 1;
    a->power_on_hours = 1;
    tab_storage_build(&p, &s);
    CHECK_STR(find_line(&p, "Error log"), "Error log       1 entry");
    CHECK_STR(find_line(&p, "Reallocated"), "Reallocated     1 sector");
    CHECK_STR(find_line(&p, "Power-on"), "Power-on        1 hour, 1 power cycle");
    CHECK_EQ(count_prefix(&p, "Power-on        1 hours"), 0);

    a->realloc = 0;
    a->uncorrectable = 3;
    tab_storage_build(&p, &s);
    CHECK_STR(find_line(&p, "Health          w"), "Health          warning (0 reallocated, 0 pending, 3 uncorrectable sectors)");

    n->crit_warning = 0x04;
    tab_storage_build(&p, &s);
    CHECK_STR(find_line(&p, "Health"), "Health          FAILING (critical warning 0x04)");
}

static void test_sensors(void) {
    static struct sensors s;
    memset(&s, 0, sizeof s);
    tab_sensors_build(&p, &s);
    CHECK_STR(p.title, "Sensors");
    CHECK_STR(find_line(&p, "No temperature"), "No temperature sensor tiny can read (virtual machine, or unsupported CPU)");
    CHECK_STR(find_line(&p, "Effective"), "Effective       not available (no APERF/MPERF)");
    CHECK_EQ(count_style(&p, STYLE_GRAPH), 0);

    // Intel: package + core, a clock still being measured.
    s.kind = SENS_INTEL;
    s.has_core = s.has_pkg = s.has_clock = true;
    s.tjmax = 100;
    s.tsc_hz = 3700000000ull;
    s.core_ok = s.pkg_ok = true;
    s.core_mc = 52000;
    s.pkg_mc = 54000;
    s.have_range = true;
    s.min_mc = 41000;
    s.max_mc = 67000;
    hist_push(&s.temp, 54);
    tab_sensors_build(&p, &s);
    CHECK_STR(find_line(&p, "Package"), "Package         54 \xf8""C (min 41, max 67)");
    CHECK_STR(find_line(&p, "Core 0"), "Core 0          52 \xf8""C");
    CHECK_STR(find_line(&p, "TjMax"), "TjMax           100 \xf8""C");
    CHECK_STR(find_line(&p, "Effective"), "Effective       measuring...");
    CHECK_STR(find_line(&p, "Base (TSC)"), "Base (TSC)      3.70 GHz");
    CHECK_EQ(count_style(&p, STYLE_GRAPH), 4);  // temperature graph only
    for (size_t i = 0; i < p.count; i++)
        if (p.lines[i].style == STYLE_GRAPH)
            CHECK_EQ((uint8_t)p.lines[i].text[0], 60);
    CHECK_STR(find_line(&p, "                last 2"), "                last 2 minutes, 20-100 \xf8""C");

    s.khz = 4650000;
    s.max_khz = 4700000;
    hist_push(&s.clock, 4650);
    s.tjmax_assumed = true;
    s.core_ok = false;  // a bad reading this second
    tab_sensors_build(&p, &s);
    CHECK_STR(find_line(&p, "Effective"), "Effective       4.65 GHz (max 4.70 GHz)");
    CHECK_STR(find_line(&p, "Core 0"), "Core 0          no reading");
    CHECK_STR(find_line(&p, "TjMax"), "TjMax           100 \xf8""C (assumed)");
    CHECK_EQ(count_style(&p, STYLE_GRAPH), 8);
    CHECK_STR(find_line(&p, "                last 2 minutes, 0"), "                last 2 minutes, 0-5.0 GHz");

    // AMD: one Tctl reading with tenths.
    memset(&s, 0, sizeof s);
    s.kind = SENS_AMD_SMN;
    s.has_pkg = true;
    s.pkg_ok = true;
    s.pkg_mc = 54125;
    s.have_range = true;
    s.min_mc = 40000;
    s.max_mc = 61875;
    tab_sensors_build(&p, &s);
    CHECK_STR(find_line(&p, "Tctl"), "Tctl            54.1 \xf8""C (min 40.0, max 61.8)");
    CHECK_EQ(count_prefix(&p, "Core 0"), 0);
    CHECK_EQ(count_prefix(&p, "TjMax"), 0);
}

static void test_usb(void) {
    static struct usb_summary s;
    memset(&s, 0, sizeof s);
    s.disabled = true;
    tab_usb_build(&p, &s);
    CHECK_STR(p.title, "USB");
    CHECK_STR(find_line(&p, "USB turned"), "USB turned off (nousb on the command line)");
    CHECK_EQ(p.lines[0].style, STYLE_WARN);

    s.disabled = false;
    s.no_dma = true;
    tab_usb_build(&p, &s);
    CHECK_STR(find_line(&p, "USB not"), "USB not started (no DMA memory)");
    s.no_dma = false;
    tab_usb_build(&p, &s);
    CHECK_STR(find_line(&p, "No xHCI"), "No xHCI controllers found (older USB controllers are not supported)");

    s.hc_count = 2;
    s.hc[0] = (struct usb_hc_summary){ "00:14.0", 0x8086, 0x0120, 16, 10, 6, USB_HC_RUNNING };
    s.hc[1] = (struct usb_hc_summary){ "03:00.0", 0x1b21, 0, 0, 0, 0, USB_HC_FAILED };
    s.dev_count = 5;
    s.dev[0] = (struct usb_dev_summary){ .hc = 0, .port = "1", .speed = USB_SPEED_HS, .has_id = true,
        .vid = 0x05e3, .pid = 0x0610, .cls = 9, .hub_ports = 4, .state = USB_DEV_HUB,
        .manufacturer = "GenesysLogic", .product = "USB2.1 Hub" };
    s.dev[1] = (struct usb_dev_summary){ .hc = 0, .port = "1.2", .speed = USB_SPEED_FS,
        .has_id = true, .vid = 0x046d, .pid = 0xc52b, .cls = 3, .sub = 1, .proto = 2,
        .state = USB_DEV_KBD_MOUSE, .manufacturer = "Logitech", .product = "Logitech USB Receiver" };
    s.dev[2] = (struct usb_dev_summary){ .hc = 0, .port = "13", .speed = USB_SPEED_SS, .has_id = true,
        .vid = 0x0781, .pid = 0x5581, .cls = 8, .sub = 6, .proto = 0x50, .state = USB_DEV_UNUSED,
        .product = "Ultra" };
    s.dev[3] = (struct usb_dev_summary){ .hc = 0, .port = "5", .speed = USB_SPEED_HS, .state = USB_DEV_NO_DESC };
    s.dev[4] = (struct usb_dev_summary){ .hc = 0, .port = "14", .speed = USB_SPEED_SS, .has_id = true,
        .vid = 0x05e3, .pid = 0x0626, .cls = 9, .state = USB_DEV_HUB_SS };
    tab_usb_build(&p, &s);
    CHECK_STR(find_line(&p, "Controllers"), "Controllers (2)");
    CHECK_STR(find_line(&p, "00:14.0"), "00:14.0  xHCI 1.20  Intel  16 ports (10 USB 2, 6 USB 3)");
    CHECK_STR(find_line(&p, "03:00.0"), "03:00.0  xHCI       ASMedia  not started (see the Log tab)");
    CHECK_STR(find_line(&p, "Devices on 00:14.0"), "Devices on 00:14.0 (5)");
    CHECK_STR(find_line(&p, "Port"), "Port    Speed   ID         Class               Status             Product");
    CHECK_STR(find_line(&p, "1   "), "1       high    05e3:0610  Hub, 4 ports        in use             GenesysLogic USB2.1 Hub");
    CHECK_STR(find_line(&p, "1.2 "), "1.2     full    046d:c52b  HID                 keyboard+mouse     Logitech USB Receiver");
    CHECK_STR(find_line(&p, "13 "), "13      super   0781:5581  Mass storage        not used           Ultra");
    CHECK_STR(find_line(&p, "5  "), "5       high    -          -                   no descriptor");
    CHECK_STR(find_line(&p, "14 "), "14      super   05e3:0626  Hub                 USB 3 half of hub");
    CHECK_STR(find_line(&p, "Devices behind"), "Devices behind a hub's USB 3 half, or behind a second hub, are not listed.");
    CHECK_STR(find_line(&p, "Devices on 03:00.0"), "");  // a controller that never started has no list
    int warns = count_style(&p, STYLE_WARN);
    CHECK_EQ(warns, 2);  // the failed controller and the device without a descriptor

    s.hc[0].state = USB_HC_STOPPED;
    s.hc[0].usb2_ports = s.hc[0].usb3_ports = 0;
    s.dev_count = 0;
    tab_usb_build(&p, &s);
    CHECK_STR(find_line(&p, "00:14.0"), "00:14.0  xHCI 1.20  Intel  16 ports, stopped (controller error)");
    CHECK_STR(find_line(&p, "Nothing"), "Nothing attached");
    CHECK_STR(find_line(&p, "Devices behind"), "");

    s.dev_count = 1;
    s.dev[0] = (struct usb_dev_summary){ .hc = 0, .port = "2", .speed = 9, .has_id = true, .vid = 1,
        .pid = 2, .cls = 0x42, .state = USB_DEV_STOPPED, .manufacturer = "Acme" };
    s.dev_truncated = true;
    tab_usb_build(&p, &s);
    CHECK_STR(find_line(&p, "2  "), "2       ?       0001:0002  Class 42            stopped (errors)   Acme");
    CHECK_STR(find_line(&p, "More than"), "More than 32 USB devices; list truncated");
}

int main(void) {
    test_usb();
    test_sensors();
    test_storage();
    test_log();
    test_fw_monitor();
    test_about();
    test_pci();
    test_cpu();
    test_mem();
    test_mem_dimms();
    test_mem_huge();
    test_fw();
    TEST_MAIN_END();
}
