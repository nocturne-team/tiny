#include "test.h"
#include "info/disk.h"

static uint16_t w[256];
static uint8_t buf[4096];

// Stores `s` the ATA way: two characters per word, first one in the high byte.
static void put_ata(int word, int nwords, const char *s) {
    size_t len = strlen(s);
    for (int i = 0; i < nwords * 2; i++) {
        uint8_t c = (size_t)i < len ? (uint8_t)s[i] : ' ';
        if (i % 2 == 0)
            w[word + i / 2] = (uint16_t)(c << 8);
        else
            w[word + i / 2] |= c;
    }
}

static void put32(uint8_t *p, uint32_t v) {
    for (int i = 0; i < 4; i++)
        p[i] = (uint8_t)(v >> (8 * i));
}

static void put64(uint8_t *p, uint64_t v) {
    for (int i = 0; i < 8; i++)
        p[i] = (uint8_t)(v >> (8 * i));
}

static void test_ata_string(void) {
    char s[41];
    memset(w, 0, sizeof w);
    put_ata(10, 10, "    QM00001");  // serials are often right-aligned
    ata_string(s, sizeof s, &w[10], 10);
    CHECK_STR(s, "QM00001");
    put_ata(27, 20, "QEMU HARDDISK");
    ata_string(s, sizeof s, &w[27], 20);
    CHECK_STR(s, "QEMU HARDDISK");
    ata_string(s, 5, &w[27], 20);  // cut to fit
    CHECK_STR(s, "QEMU");
    w[0] = 0x4100;  // "A\0": NUL and other junk become '?'
    ata_string(s, sizeof s, &w[0], 1);
    CHECK_STR(s, "A?");
}

static void test_ata_identify_lba28(void) {
    struct disk_info d;
    disk_init(&d, DISK_SATA);
    memset(w, 0, sizeof w);
    put_ata(10, 10, "SER123");
    put_ata(23, 4, "FW1");
    put_ata(27, 20, "Old Disk");
    w[60] = 0x5000;  // 0x00105000 sectors
    w[61] = 0x0010;
    w[82] = 1;       // SMART supported, not enabled
    ata_parse_identify(w, &d);
    CHECK_STR(d.model, "Old Disk");
    CHECK_STR(d.serial, "SER123");
    CHECK_STR(d.firmware, "FW1");
    CHECK_EQ(d.bytes, 0x00105000ull * 512);
    CHECK_EQ(d.sector, 512);
    CHECK_EQ(d.rpm, 0);
    CHECK(d.smart_supported);
    CHECK(!d.smart_enabled);
}

static void test_ata_identify_lba48_4k(void) {
    struct disk_info d;
    disk_init(&d, DISK_SATA);
    memset(w, 0, sizeof w);
    w[60] = 0xFFFF;  // LBA28 field saturated
    w[61] = 0x0FFF;
    w[83] = 0x4000 | 1 << 10; // valid (bits 15:14 = 01), LBA48
    w[86] = 1 << 10;
    w[100] = 0x0000; // 0x0000_0001_0000_0000 sectors
    w[101] = 0x0000;
    w[102] = 0x0001;
    w[103] = 0x0000;
    w[106] = (1 << 14) | (1 << 12);  // valid, logical sector > 512 bytes
    w[117] = 2048;   // in words: 4096 bytes
    w[118] = 0;
    w[82] = 1;
    w[85] = 1;
    w[217] = 1;      // non-rotating
    ata_parse_identify(w, &d);
    CHECK_EQ(d.sector, 4096);
    CHECK_EQ(d.bytes, 0x100000000ull * 4096);
    CHECK_EQ(d.rpm, 1);
    CHECK(d.smart_supported && d.smart_enabled);

    w[217] = 7200;
    w[106] = 0;      // word 106 not valid: 512-byte sectors
    ata_parse_identify(w, &d);
    CHECK_EQ(d.sector, 512);
    CHECK_EQ(d.rpm, 7200);
    w[217] = 0xFFFF; // reserved value
    ata_parse_identify(w, &d);
    CHECK_EQ(d.rpm, 0);

    w[83] = 0xFFFF;  // garbage word 83 (validity bits wrong): LBA48 words ignored
    ata_parse_identify(w, &d);
    CHECK_EQ(d.bytes, 0x0FFFFFFFull * 512);
}

static void smart_attr(int slot, uint8_t id, uint64_t raw) {
    uint8_t *a = &buf[2 + slot * 12];
    a[0] = id;
    a[3] = 100;  // current
    a[4] = 100;  // worst
    for (int i = 0; i < 6; i++)
        a[5 + i] = (uint8_t)(raw >> (8 * i));
}

static void smart_checksum(void) {
    uint8_t sum = 0;
    for (int i = 0; i < 511; i++)
        sum += buf[i];
    buf[511] = (uint8_t)-sum;
}

static void test_ata_smart(void) {
    struct disk_info d;
    disk_init(&d, DISK_SATA);
    memset(buf, 0, 512);
    buf[0] = 0x10;  // version
    smart_attr(0, 5, 8);
    smart_attr(1, 9, 0x0012000004D2ull);  // vendors stuff minutes in the high bytes
    smart_attr(2, 12, 321);
    smart_attr(3, 194, 0x003C00140023ull); // 35 C, plus min/max in the high bytes
    smart_attr(4, 197, 2);
    smart_attr(5, 198, 0);
    smart_checksum();
    CHECK(ata_parse_smart(buf, &d));
    CHECK(d.smart);
    CHECK_EQ(d.realloc, 8);
    CHECK_EQ(d.power_on_hours, 1234);
    CHECK_EQ(d.power_cycles, 321);
    CHECK(d.has_temp);
    CHECK_EQ(d.temp_c, 35);
    CHECK_EQ(d.pending, 2);
    CHECK_EQ(d.uncorrectable, 0);

    disk_init(&d, DISK_SATA);
    buf[511] ^= 1;  // bad checksum: ignored
    CHECK(!ata_parse_smart(buf, &d));
    CHECK(!d.smart);
    CHECK_EQ(d.realloc, DISK_NONE);

    disk_init(&d, DISK_SATA);  // attributes the drive doesn't have stay absent
    memset(buf, 0, 512);
    smart_attr(0, 9, 10);
    smart_checksum();
    CHECK(ata_parse_smart(buf, &d));
    CHECK_EQ(d.power_on_hours, 10);
    CHECK_EQ(d.realloc, DISK_NONE);
    CHECK(!d.has_temp);
}

static void test_nvme_ctrl(void) {
    struct disk_info d;
    disk_init(&d, DISK_NVME);
    memset(buf, 0, sizeof buf);
    memcpy(&buf[4], "S5GXNX0R123456      ", 20);
    memcpy(&buf[24], "Samsung SSD 980 PRO 1TB                 ", 40);
    memcpy(&buf[64], "5B2QGXA7", 8);
    put32(&buf[516], 3);
    CHECK_EQ(nvme_parse_ctrl(buf, &d), 3);
    CHECK_STR(d.serial, "S5GXNX0R123456");
    CHECK_STR(d.model, "Samsung SSD 980 PRO 1TB");
    CHECK_STR(d.firmware, "5B2QGXA7");
}

static void test_nvme_ns(void) {
    uint32_t sector = 0;
    memset(buf, 0, sizeof buf);
    put64(&buf[0], 1953525168);  // NSZE
    buf[26] = 1;                 // FLBAS: format 1
    buf[128 + 0 * 4 + 2] = 9;    // format 0: 512 bytes
    buf[128 + 1 * 4 + 2] = 12;   // format 1: 4096 bytes
    CHECK_EQ(nvme_ns_bytes(buf, &sector), 1953525168ull * 4096);
    CHECK_EQ(sector, 4096);

    buf[26] = 0x10;  // metadata bit set, format 0
    CHECK_EQ(nvme_ns_bytes(buf, &sector), 1953525168ull * 512);
    CHECK_EQ(sector, 512);

    buf[128 + 2] = 3;  // nonsense LBA size
    CHECK_EQ(nvme_ns_bytes(buf, &sector), 0);
    put64(&buf[0], 0);  // inactive namespace
    buf[128 + 2] = 9;
    CHECK_EQ(nvme_ns_bytes(buf, &sector), 0);
}

static void test_nvme_smart(void) {
    struct disk_info d;
    disk_init(&d, DISK_NVME);
    memset(buf, 0, 512);
    buf[0] = 0;
    buf[1] = 0x37;  // 311 K = 38 C
    buf[2] = 0x01;
    buf[3] = 100;
    buf[4] = 10;
    buf[5] = 3;
    put64(&buf[32], 1000);       // data units read (x 512000 bytes)
    buf[32 + 8] = 1;             // high half set: saturates
    put64(&buf[48], 2000);
    put64(&buf[112], 567);
    put64(&buf[128], 1234);
    put64(&buf[144], 42);
    put64(&buf[160], 0);
    put64(&buf[176], 7);
    nvme_parse_smart(buf, &d);
    CHECK(d.smart);
    CHECK(d.has_temp);
    CHECK_EQ(d.temp_c, 38);
    CHECK_EQ(d.spare, 100);
    CHECK_EQ(d.spare_thresh, 10);
    CHECK_EQ(d.used_pct, 3);
    CHECK_EQ(d.bytes_read, DISK_NONE - 1);
    CHECK_EQ(d.bytes_written, 2000ull * 512000);
    CHECK_EQ(d.power_cycles, 567);
    CHECK_EQ(d.power_on_hours, 1234);
    CHECK_EQ(d.unsafe_shutdowns, 42);
    CHECK_EQ(d.media_errors, 0);
    CHECK_EQ(d.error_entries, 7);

    buf[1] = buf[2] = 0;  // temperature not reported
    disk_init(&d, DISK_NVME);
    nvme_parse_smart(buf, &d);
    CHECK(!d.has_temp);
    buf[1] = 0xEE;        // 65262 K: garbage
    buf[2] = 0xFE;
    nvme_parse_smart(buf, &d);
    CHECK(!d.has_temp);
    buf[1] = 100;         // 100 K: garbage too
    buf[2] = 0;
    nvme_parse_smart(buf, &d);
    CHECK(!d.has_temp);
}

static void test_health(void) {
    struct disk_info d;
    disk_init(&d, DISK_NVME);
    CHECK_EQ(disk_health(&d), HEALTH_UNKNOWN);
    d.smart = true;
    d.spare = 100;
    d.spare_thresh = 10;
    CHECK_EQ(disk_health(&d), HEALTH_OK);
    d.crit_warning = 0x02;  // temperature
    CHECK_EQ(disk_health(&d), HEALTH_WARNING);
    d.crit_warning = 0x10;  // volatile memory backup
    CHECK_EQ(disk_health(&d), HEALTH_WARNING);
    d.crit_warning = 0x04;  // reliability degraded
    CHECK_EQ(disk_health(&d), HEALTH_FAILING);
    d.crit_warning = 0x08;  // read-only
    CHECK_EQ(disk_health(&d), HEALTH_FAILING);
    d.crit_warning = 0;
    d.spare = 5;            // below threshold
    CHECK_EQ(disk_health(&d), HEALTH_FAILING);
    d.spare = 100;
    d.used_pct = 100;
    CHECK_EQ(disk_health(&d), HEALTH_WORN);

    disk_init(&d, DISK_SATA);
    CHECK_EQ(disk_health(&d), HEALTH_UNKNOWN);
    d.smart_supported = d.smart_enabled = true;
    d.smart = true;
    d.realloc = d.pending = d.uncorrectable = 0;
    CHECK_EQ(disk_health(&d), HEALTH_OK);
    d.pending = 1;
    CHECK_EQ(disk_health(&d), HEALTH_WARNING);
    d.pending = DISK_NONE;  // absent counters don't count
    CHECK_EQ(disk_health(&d), HEALTH_OK);
    d.smart_failed = true;
    CHECK_EQ(disk_health(&d), HEALTH_FAILING);

    disk_init(&d, DISK_SATA);  // status says passed but attributes unreadable: still ok
    d.smart_supported = d.smart_enabled = true;
    d.smart = false;
    CHECK_EQ(disk_health(&d), HEALTH_UNKNOWN);

    disk_init(&d, DISK_ATAPI);
    CHECK_EQ(disk_health(&d), HEALTH_UNKNOWN);

    CHECK_STR(disk_health_name(HEALTH_OK), "OK");
    CHECK_STR(disk_health_name(HEALTH_FAILING), "FAILING");
}

int main(void) {
    test_ata_string();
    test_ata_identify_lba28();
    test_ata_identify_lba48_4k();
    test_ata_smart();
    test_nvme_ctrl();
    test_nvme_ns();
    test_nvme_smart();
    test_health();
    TEST_MAIN_END();
}
