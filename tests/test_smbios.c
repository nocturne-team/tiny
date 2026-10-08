#include "test.h"
#include "info/smbios.h"
#include <stdint.h>
#include <stdlib.h>

#define PHYS_BASE 0x200000ULL
#define TABLE_OFF 512
static uint8_t arena[4096];
static uint8_t *tab;
static size_t tlen;

static uint64_t hhdm(void) { return (uint64_t)(uintptr_t)arena - PHYS_BASE; }
static void put16(uint8_t *p, uint16_t v) { memcpy(p, &v, 2); }
static void put32(uint8_t *p, uint32_t v) { memcpy(p, &v, 4); }
static void put64(uint8_t *p, uint64_t v) { memcpy(p, &v, 8); }

static void fix_csum(uint8_t *p, size_t len, size_t csum_off) {
    p[csum_off] = 0;
    uint8_t sum = 0;
    for (size_t i = 0; i < len; i++)
        sum = (uint8_t)(sum + p[i]);
    p[csum_off] = (uint8_t)(0 - sum);
}

static uint8_t *begin_struct(uint8_t type, uint8_t flen) {
    uint8_t *s = tab + tlen;
    memset(s, 0, flen);
    s[0] = type;
    s[1] = flen;
    s[2] = (uint8_t)tlen;  // handle
    tlen += flen;
    return s;
}

static void strings(const char *const *list, size_t n) {
    if (n == 0) {
        tab[tlen++] = 0;
        tab[tlen++] = 0;
        return;
    }
    for (size_t i = 0; i < n; i++) {
        size_t l = strlen(list[i]) + 1;
        memcpy(tab + tlen, list[i], l);
        tlen += l;
    }
    tab[tlen++] = 0;
}

static size_t type1_off;  // offset of the type 1 structure within the table

static void build_table(const char *sys_vendor) {
    memset(arena, 0, sizeof arena);
    tab = arena + TABLE_OFF;
    tlen = 0;

    uint8_t *s = begin_struct(0, 0x18);
    s[4] = 1; s[5] = 2; s[8] = 3;
    const char *bios[] = { "American Megatrends Inc.", "1.23   ", "01/02/2024" };
    strings(bios, 3);

    type1_off = tlen;
    s = begin_struct(1, 0x1B);
    s[4] = 1; s[5] = 2; s[6] = 3;
    const char *sys[] = { sys_vendor, "Standard PC (Q35 + ICH9, 2009)", "pc-q35-10.2" };
    strings(sys, 3);

    begin_struct(2, 0x0F);  // all string indices 0
    strings(NULL, 0);

    s = begin_struct(4, 0x30);
    s[4] = 1;
    const char *cpu[] = { "CPU 0" };
    strings(cpu, 1);

    begin_struct(127, 4);
    strings(NULL, 0);
}

static uint8_t *build_ep32(size_t off, uint64_t table_phys, uint16_t table_len) {
    uint8_t *e = arena + off;
    memset(e, 0, 0x1F);
    memcpy(e, "_SM_", 4);
    e[5] = 0x1F;
    e[6] = 2;
    e[7] = 8;
    memcpy(e + 16, "_DMI_", 5);
    put16(e + 22, table_len);
    put32(e + 24, (uint32_t)table_phys);
    put16(e + 28, 5);
    fix_csum(e + 16, 15, 5);
    fix_csum(e, 0x1F, 4);
    return e;
}

static uint8_t *build_ep64(size_t off) {
    uint8_t *e = arena + off;
    memset(e, 0, 0x18);
    memcpy(e, "_SM3_", 5);
    e[6] = 0x18;
    e[7] = 3;
    e[8] = 5;
    e[10] = 1;
    put32(e + 12, (uint32_t)tlen);
    put64(e + 16, PHYS_BASE + TABLE_OFF);
    fix_csum(e, 0x18, 5);
    return e;
}

static struct smbios_info s;

static void test_ep32(void) {
    build_table("QEMU");
    uint8_t *e = build_ep32(0, PHYS_BASE + TABLE_OFF, (uint16_t)tlen);
    smbios_parse(e, NULL, hhdm(), &s);
    CHECK(s.present);
    CHECK(!s.is64);
    CHECK(s.checksum_ok);
    CHECK_EQ(s.major, 2);
    CHECK_EQ(s.minor, 8);
    CHECK_STR(s.bios_vendor, "American Megatrends Inc.");
    CHECK_STR(s.bios_version, "1.23");  // trailing spaces trimmed
    CHECK_STR(s.bios_date, "01/02/2024");
    CHECK_STR(s.sys_vendor, "QEMU");
    CHECK_STR(s.sys_product, "Standard PC (Q35 + ICH9, 2009)");
    CHECK_STR(s.sys_version, "pc-q35-10.2");
    CHECK_STR(s.board_vendor, "");
    CHECK_STR(s.board_product, "");
}

static void test_ep64_preferred(void) {
    build_table("QEMU");
    uint8_t *e32 = build_ep32(0, PHYS_BASE + TABLE_OFF, (uint16_t)tlen);
    uint8_t *e64 = build_ep64(64);
    smbios_parse(e32, e64, hhdm(), &s);
    CHECK(s.is64);
    CHECK_EQ(s.major, 3);
    CHECK_EQ(s.minor, 5);
    CHECK_STR(s.sys_vendor, "QEMU");
}

static void test_bad_checksum(void) {
    build_table("QEMU");
    uint8_t *e = build_ep32(0, PHYS_BASE + TABLE_OFF, (uint16_t)tlen);
    e[10] ^= 0x55;
    smbios_parse(e, NULL, hhdm(), &s);
    CHECK(s.present);
    CHECK(!s.checksum_ok);
    CHECK_STR(s.sys_vendor, "QEMU");
}

static void test_missing(void) {
    smbios_parse(NULL, NULL, 0, &s);
    CHECK(!s.present);
    memset(arena, 0, sizeof arena);
    memcpy(arena, "_XX_", 4);
    smbios_parse(arena, NULL, hhdm(), &s);
    CHECK(!s.present);
}

static void test_bad_string_index(void) {
    build_table("QEMU");
    tab[type1_off + 5] = 9;  // product -> string 9, which does not exist
    uint8_t *e = build_ep32(0, PHYS_BASE + TABLE_OFF, (uint16_t)tlen);
    smbios_parse(e, NULL, hhdm(), &s);
    CHECK_STR(s.sys_product, "");
    CHECK_STR(s.sys_vendor, "QEMU");
}

static void test_garbage_strings(void) {
    build_table("\x01QEMU\x7f with a very long vendor name that keeps_going_and_going");
    uint8_t *e = build_ep32(0, PHYS_BASE + TABLE_OFF, (uint16_t)tlen);
    smbios_parse(e, NULL, hhdm(), &s);
    CHECK_EQ(strlen(s.sys_vendor), sizeof s.sys_vendor - 1);
    CHECK(strncmp(s.sys_vendor, "?QEMU? with", 11) == 0);
}

static void test_truncated_table(void) {
    // Cut the table in the middle of the type 1 strings and copy it into an
    // exact-size heap buffer so ASan catches any read past the end.
    build_table("QEMU");
    size_t cut = type1_off + 0x1B + 6;
    uint8_t *heap = malloc(cut);
    memcpy(heap, tab, cut);
    uint64_t fake_phys = 0x300000;
    uint64_t heap_hhdm = (uint64_t)(uintptr_t)heap - fake_phys;

    uint8_t ep[0x1F];
    memset(ep, 0, sizeof ep);
    memcpy(ep, "_SM_", 4);
    ep[5] = 0x1F; ep[6] = 2; ep[7] = 8;
    memcpy(ep + 16, "_DMI_", 5);
    put16(ep + 22, (uint16_t)cut);
    put32(ep + 24, (uint32_t)fake_phys);
    fix_csum(ep, 0x1F, 4);

    smbios_parse(ep, NULL, heap_hhdm, &s);
    CHECK(s.present);
    CHECK_STR(s.bios_vendor, "American Megatrends Inc.");
    CHECK_STR(s.sys_vendor, "");  // structure dropped: its string set never ends
    free(heap);
}

static uint8_t *dimm(uint8_t flen, uint16_t size, uint8_t type, uint16_t speed,
                     const char *locator, const char *maker, const char *part) {
    uint8_t *d = begin_struct(17, flen);
    put16(d + 0x0C, size);
    d[0x10] = 1;  // locator -> string 1
    d[0x12] = type;
    if (flen >= 0x17)
        put16(d + 0x15, speed);
    if (flen >= 0x1B) {
        d[0x17] = maker ? 2 : 0;
        d[0x1A] = part ? 3 : 0;
    }
    const char *strs[3] = { locator, maker ? maker : "", part ? part : "" };
    strings(strs, maker || part ? 3 : 1);
    return d;
}

static void parse_table(void) {
    uint8_t *e = build_ep32(0, PHYS_BASE + TABLE_OFF, (uint16_t)tlen);
    smbios_parse(e, NULL, hhdm(), &s);
}

static void start_table(void) {
    memset(arena, 0, sizeof arena);
    tab = arena + TABLE_OFF;
    tlen = 0;
}

static void test_dimms(void) {
    start_table();
    dimm(0x28, 8192, 0x1A, 3200, "DIMM_A1", "Kingston", "KF432C16BB/8");  // 8 GiB DDR4
    dimm(0x28, 0, 0x02, 0, "DIMM_A2", NULL, NULL);                          // empty slot
    uint8_t *big = dimm(0x5C, 0x7FFF, 0x22, 0xFFFF, "DIMM_B1", "Samsung", "M321R8GA0BB0");
    put32(big + 0x1C, 0x10000);  // extended size: 64 GiB in MB
    put32(big + 0x54, 6000);     // extended speed
    dimm(0x15, 0x8000 | 512, 0x07, 0, "SIMM0", NULL, NULL);                 // 512 KB, SMBIOS 2.1 layout
    begin_struct(127, 4);
    strings(NULL, 0);
    parse_table();

    CHECK_EQ(s.dimm_count, 4);
    CHECK(!s.dimms_truncated);
    CHECK_STR(s.dimms[0].locator, "DIMM_A1");
    CHECK_EQ(s.dimms[0].size, 8192ULL << 20);
    CHECK_EQ(s.dimms[0].type, 0x1A);
    CHECK_EQ(s.dimms[0].speed_mts, 3200);
    CHECK_STR(s.dimms[0].manufacturer, "Kingston");
    CHECK_STR(s.dimms[0].part, "KF432C16BB/8");
    CHECK_EQ(s.dimms[1].size, 0);
    CHECK(!s.dimms[1].size_unknown);
    CHECK_STR(s.dimms[1].manufacturer, "");
    CHECK_EQ(s.dimms[2].size, 64ULL << 30);
    CHECK_EQ(s.dimms[2].speed_mts, 6000);
    CHECK_EQ(s.dimms[3].size, 512ULL << 10);
    CHECK_EQ(s.dimms[3].speed_mts, 0);
    CHECK_STR(s.dimms[3].locator, "SIMM0");
}

static void test_dimm_unknown_and_truncated(void) {
    start_table();
    dimm(0x28, 0xFFFF, 0x02, 0, "X", NULL, NULL);
    for (int i = 0; i < 20; i++)
        dimm(0x28, 1024, 0x18, 1600, "D", NULL, NULL);
    begin_struct(127, 4);
    strings(NULL, 0);
    parse_table();
    CHECK(s.dimms[0].size_unknown);
    CHECK_EQ(s.dimm_count, SMBIOS_MAX_DIMMS);
    CHECK(s.dimms_truncated);
}

static void test_memory_type_names(void) {
    CHECK_STR(smbios_memory_type_name(0x1A), "DDR4");
    CHECK_STR(smbios_memory_type_name(0x22), "DDR5");
    CHECK_STR(smbios_memory_type_name(0x23), "LPDDR5");
    CHECK_STR(smbios_memory_type_name(0x18), "DDR3");
    CHECK_STR(smbios_memory_type_name(0x07), "RAM");
    CHECK_STR(smbios_memory_type_name(0x02), "unknown");
    CHECK_STR(smbios_memory_type_name(0x99), "unknown");
}

int main(void) {
    test_dimms();
    test_dimm_unknown_and_truncated();
    test_memory_type_names();
    test_ep32();
    test_ep64_preferred();
    test_bad_checksum();
    test_missing();
    test_bad_string_index();
    test_garbage_strings();
    test_truncated_table();
    TEST_MAIN_END();
}
