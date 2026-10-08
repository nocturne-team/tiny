#include "test.h"
#include "info/acpi.h"
#include <stdint.h>

#define PHYS_BASE 0x100000ULL
static uint8_t arena[8192] __attribute__((aligned(16)));

static uint64_t hhdm(void) { return (uint64_t)(uintptr_t)arena - PHYS_BASE; }
static uint64_t phys(size_t off) { return PHYS_BASE + off; }
static void put32(uint8_t *p, uint32_t v) { memcpy(p, &v, 4); }
static void put64(uint8_t *p, uint64_t v) { memcpy(p, &v, 8); }

static void fix_csum(uint8_t *p, size_t len, size_t csum_off) {
    p[csum_off] = 0;
    uint8_t sum = 0;
    for (size_t i = 0; i < len; i++)
        sum = (uint8_t)(sum + p[i]);
    p[csum_off] = (uint8_t)(0 - sum);
}

static uint8_t *table(size_t off, const char *sig, uint32_t len) {
    uint8_t *t = arena + off;
    memset(t, 0, len);
    memcpy(t, sig, 4);
    put32(t + 4, len);
    t[8] = 1;
    memcpy(t + 10, "TINYOS", 6);
    return t;
}

// RSDP @0, XSDT @64, RSDT @128, FACP @256 (276 bytes), APIC @640, DSDT @768
static void build(int rsdp_rev) {
    memset(arena, 0, sizeof arena);
    uint8_t *rsdp = arena;
    memcpy(rsdp, "RSD PTR ", 8);
    memcpy(rsdp + 9, "TINYOS", 6);
    rsdp[15] = (uint8_t)rsdp_rev;
    put32(rsdp + 16, (uint32_t)phys(128));
    fix_csum(rsdp, 20, 8);
    if (rsdp_rev >= 2) {
        put32(rsdp + 20, 36);
        put64(rsdp + 24, phys(64));
        fix_csum(rsdp, 36, 32);
    }

    uint8_t *xsdt = table(64, "XSDT", 36 + 16);
    put64(xsdt + 36, phys(256));
    put64(xsdt + 44, phys(640));
    fix_csum(xsdt, 52, 9);

    uint8_t *rsdt = table(128, "RSDT", 36 + 8);
    put32(rsdt + 36, (uint32_t)phys(256));
    put32(rsdt + 40, (uint32_t)phys(640));
    fix_csum(rsdt, 44, 9);

    uint8_t *facp = table(256, "FACP", 276);
    put32(facp + 40, (uint32_t)phys(768));  // DSDT
    put64(facp + 140, phys(768));           // X_DSDT
    fix_csum(facp, 276, 9);

    uint8_t *apic = table(640, "APIC", 44);
    fix_csum(apic, 44, 9);

    static const uint8_t aml[] = { 0x08, '_', 'S', '5', '_', 0x12, 0x08, 0x04, 0x0A, 0x05, 0x0A, 0x07, 0x00, 0x00 };
    uint8_t *dsdt = table(768, "DSDT", 36 + sizeof aml);
    memcpy(dsdt + 36, aml, sizeof aml);
    fix_csum(dsdt, 36 + sizeof aml, 9);
}

static struct acpi_info a;

static void test_xsdt(void) {
    build(2);
    acpi_parse(arena, hhdm(), &a);
    CHECK(a.present);
    CHECK_EQ(a.revision, 2);
    CHECK(a.use_xsdt);
    CHECK(a.rsdp_checksum_ok);
    CHECK_STR(a.oem_id, "TINYOS");
    CHECK_EQ(a.table_count, 4);
    CHECK_STR(a.tables[0].sig, "XSDT");
    CHECK_STR(a.tables[1].sig, "FACP");
    CHECK_STR(a.tables[2].sig, "APIC");
    CHECK_STR(a.tables[3].sig, "DSDT");
    for (size_t i = 0; i < a.table_count; i++)
        CHECK(a.tables[i].checksum_ok);
    CHECK_EQ(a.tables[1].length, 276);
    CHECK_STR(a.tables[2].oem_id, "TINYOS");
    CHECK(acpi_find(&a, "APIC") == &a.tables[2]);
    CHECK(acpi_find(&a, "HPET") == NULL);
    CHECK(!a.truncated);
}

static void test_rsdt(void) {
    build(0);
    acpi_parse(arena, hhdm(), &a);
    CHECK(a.present);
    CHECK(!a.use_xsdt);
    CHECK_EQ(a.table_count, 4);
    CHECK_STR(a.tables[0].sig, "RSDT");
    CHECK_STR(a.tables[3].sig, "DSDT");
}

static void test_bad_checksums(void) {
    build(2);
    arena[640 + 20] ^= 0xFF;  // corrupt APIC body
    arena[8] ^= 0x01;         // corrupt RSDP checksum
    acpi_parse(arena, hhdm(), &a);
    CHECK(a.present);
    CHECK(!a.rsdp_checksum_ok);
    CHECK_EQ(a.table_count, 4);
    CHECK(!a.tables[2].checksum_ok);
    CHECK(a.tables[1].checksum_ok);
}

static void test_bogus_root_length(void) {
    build(2);
    put32(arena + 64 + 4, 0xFFFFFFFF);
    acpi_parse(arena, hhdm(), &a);
    CHECK(a.present);
    CHECK_EQ(a.table_count, 1);  // only the root itself, flagged
    CHECK(!a.tables[0].checksum_ok);
}

static void test_truncation(void) {
    build(0);
    uint8_t *rsdt = table(1024, "RSDT", 36 + 4 * 100);
    for (int i = 0; i < 100; i++)
        put32(rsdt + 36 + 4 * i, (uint32_t)phys(640));
    fix_csum(rsdt, 36 + 400, 9);
    put32(arena + 16, (uint32_t)phys(1024));
    fix_csum(arena, 20, 8);
    acpi_parse(arena, hhdm(), &a);
    CHECK(a.truncated);
    CHECK_EQ(a.table_count, ACPI_MAX_TABLES);
}

static void test_missing(void) {
    acpi_parse(NULL, 0, &a);
    CHECK(!a.present);
    memset(arena, 0, sizeof arena);
    acpi_parse(arena, hhdm(), &a);
    CHECK(!a.present);
    CHECK(acpi_find(&a, "FACP") == NULL);
}

static void test_sanitize(void) {
    build(2);
    arena[640 + 10] = 0x07;  // control byte in APIC OEM ID
    fix_csum(arena + 640, 44, 9);
    acpi_parse(arena, hhdm(), &a);
    CHECK_STR(a.tables[2].oem_id, "?INYOS");
}

static void test_s5(void) {
    uint8_t ta, tb;
    static const uint8_t byteprefix[] = { 0x10, 0x08, '_', 'S', '5', '_', 0x12, 0x08, 0x04, 0x0A, 0x05, 0x0A, 0x07, 0x00, 0x00 };
    CHECK(acpi_find_s5(byteprefix, sizeof byteprefix, &ta, &tb));
    CHECK_EQ(ta, 5);
    CHECK_EQ(tb, 7);

    static const uint8_t rootprefix[] = { 0x08, '\\', '_', 'S', '5', '_', 0x12, 0x06, 0x04, 0x01, 0x00, 0x00, 0x00 };
    CHECK(acpi_find_s5(rootprefix, sizeof rootprefix, &ta, &tb));
    CHECK_EQ(ta, 1);
    CHECK_EQ(tb, 0);

    static const uint8_t zeros[] = { 0x08, '_', 'S', '5', '_', 0x12, 0x06, 0x04, 0x00, 0x00, 0x00, 0x00 };
    CHECK(acpi_find_s5(zeros, sizeof zeros, &ta, &tb));
    CHECK_EQ(ta, 0);
    CHECK_EQ(tb, 0);

    static const uint8_t not_a_name_first[] = { 0x14, '_', 'S', '5', '_', 0x00,
        0x08, '_', 'S', '5', '_', 0x12, 0x06, 0x02, 0x0A, 0x03, 0x0A, 0x04 };
    CHECK(acpi_find_s5(not_a_name_first, sizeof not_a_name_first, &ta, &tb));
    CHECK_EQ(ta, 3);
    CHECK_EQ(tb, 4);

    static const uint8_t wide[] = { 0x08, '_', 'S', '5', '_', 0x12, 0x0A, 0x02,
        0x0B, 0x05, 0x00, 0x0C, 0x06, 0x00, 0x00, 0x00 };
    CHECK(acpi_find_s5(wide, sizeof wide, &ta, &tb));
    CHECK_EQ(ta, 5);
    CHECK_EQ(tb, 6);

    static const uint8_t pkglen2[] = { 0x08, '_', 'S', '5', '_', 0x12, 0x40, 0x01, 0x04, 0x0A, 0x02, 0x0A, 0x03 };
    CHECK(acpi_find_s5(pkglen2, sizeof pkglen2, &ta, &tb));
    CHECK_EQ(ta, 2);
    CHECK_EQ(tb, 3);

    static const uint8_t truncated[] = { 0x08, '_', 'S', '5', '_', 0x12 };
    CHECK(!acpi_find_s5(truncated, sizeof truncated, &ta, &tb));
    static const uint8_t none[] = { 0x08, '_', 'S', '4', '_', 0x12, 0x06 };
    CHECK(!acpi_find_s5(none, sizeof none, &ta, &tb));
}

int main(void) {
    test_xsdt();
    test_rsdt();
    test_bad_checksums();
    test_bogus_root_length();
    test_truncation();
    test_missing();
    test_sanitize();
    test_s5();
    TEST_MAIN_END();
}
