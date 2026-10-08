#include "test.h"
#include "info/cpu.h"

static void test_signature(void) {
    struct cpu_sig s;
    cpu_decode_signature(0x000906EA, &s);  // Core i7-8700K
    CHECK_EQ(s.family, 6);
    CHECK_EQ(s.model, 158);
    CHECK_EQ(s.stepping, 10);
    cpu_decode_signature(0x00870F10, &s);  // Ryzen 5 3600
    CHECK_EQ(s.family, 23);
    CHECK_EQ(s.model, 113);
    CHECK_EQ(s.stepping, 0);
    cpu_decode_signature(0x00A20F12, &s);  // Ryzen 9 5900X
    CHECK_EQ(s.family, 25);
    CHECK_EQ(s.model, 33);
    CHECK_EQ(s.stepping, 2);
    cpu_decode_signature(0x00000F41, &s);  // Pentium 4
    CHECK_EQ(s.family, 15);
    CHECK_EQ(s.model, 4);
    CHECK_EQ(s.stepping, 1);
    cpu_decode_signature(0x00010633, &s);  // family 6: extended model applies
    CHECK_EQ(s.model, 0x13);
    cpu_decode_signature(0x00010533, &s);  // family 5: extended model ignored
    CHECK_EQ(s.family, 5);
    CHECK_EQ(s.model, 3);
}

static void test_features(void) {
    struct cpu_feature_regs r = { 0 };
    r.l1_edx = (1u << 25) | (1u << 26);  // sse sse2
    r.l1_ecx = 1u << 28;                 // avx
    r.e1_edx = 1u << 29;                 // lm
    const char *names[16];
    size_t n = cpu_feature_names(&r, names, 16);
    CHECK_EQ(n, 4);
    CHECK_STR(names[0], "sse");
    CHECK_STR(names[1], "sse2");
    CHECK_STR(names[2], "avx");
    CHECK_STR(names[3], "lm");
    CHECK_EQ(cpu_feature_names(&r, names, 2), 2);  // respects max

    struct cpu_feature_regs none = { 0 };
    CHECK_EQ(cpu_feature_names(&none, names, 16), 0);
}

static void test_cache(void) {
    struct cache_desc c;
    // L1 data: 8 ways x 1 partition x 64-byte lines x 64 sets = 32 KiB
    CHECK(cpu_decode_cache(0x21, (7u << 22) | 63, 63, &c));
    CHECK_EQ(c.level, 1);
    CHECK_EQ(c.type, CACHE_DATA);
    CHECK_EQ(c.size, 32768);
    CHECK(cpu_decode_cache(0x22, (7u << 22) | 63, 63, &c));
    CHECK_EQ(c.type, CACHE_INSN);
    // L3 unified: 16 ways x 64 B x 12288 sets = 12 MiB
    CHECK(cpu_decode_cache(0x63, (15u << 22) | 63, 12287, &c));
    CHECK_EQ(c.level, 3);
    CHECK_EQ(c.type, CACHE_UNIFIED);
    CHECK_EQ(c.size, 12582912);
    CHECK(!cpu_decode_cache(0, 0, 0, &c));  // terminating entry
}

static void test_amd_legacy_cache(void) {
    struct cache_desc c[4];
    // 0x80000005 ECX/EDX: L1d/L1i KB in bits 31-24; 0x80000006 ECX: L2 KB in 31-16,
    // EDX: L3 in 512 KB units in bits 31-18.
    size_t n = cpu_decode_amd_legacy_caches(0x40020140, 0x40020140, 0x02008140, 0x00088140, c, 4);
    CHECK_EQ(n, 4);
    CHECK_EQ(c[0].level, 1);
    CHECK_EQ(c[0].type, CACHE_DATA);
    CHECK_EQ(c[0].size, 64 * 1024);
    CHECK_EQ(c[1].type, CACHE_INSN);
    CHECK_EQ(c[2].level, 2);
    CHECK_EQ(c[2].type, CACHE_UNIFIED);
    CHECK_EQ(c[2].size, 512 * 1024);
    CHECK_EQ(c[3].level, 3);
    CHECK_EQ(c[3].size, 1024 * 1024);
    CHECK_EQ(cpu_decode_amd_legacy_caches(0x40020140, 0, 0, 0, c, 4), 1);  // zero sizes skipped
    CHECK_EQ(cpu_decode_amd_legacy_caches(0x40020140, 0x40020140, 0x02008140, 0, c, 2), 2);  // max
}

int main(void) {
    test_amd_legacy_cache();
    test_signature();
    test_features();
    test_cache();
    TEST_MAIN_END();
}
