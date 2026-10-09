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

static int find(const char **names, size_t n, const char *want) {
    for (size_t i = 0; i < n; i++)
        if (strcmp(names[i], want) == 0)
            return (int)i;
    return -1;
}

static void test_security(void) {
    // Microcode revision: Intel in the high half of MSR 0x8B, AMD in the low half.
    CHECK_EQ(cpu_microcode_rev(0x000000F400000000ull, false), 0xF4);
    CHECK_EQ(cpu_microcode_rev(0x12345678ull | (0xFFull << 32), true), 0x12345678);

    const char *n[24];
    struct cpu_feature_regs r = { 0 };
    // Intel: IA32_ARCH_CAPABILITIES RDCL_NO, MDS_NO, GDS_NO.
    size_t k = cpu_immunities(false, &r, (1ull << 0) | (1ull << 5) | (1ull << 26), n, 24);
    CHECK_EQ(k, 4);
    CHECK_STR(n[0], "Meltdown");
    CHECK_STR(n[1], "L1TF");
    CHECK_STR(n[2], "MDS");
    CHECK_STR(n[3], "Downfall (GDS)");
    CHECK_EQ(cpu_immunities(false, &r, 0, n, 24), 0);
    // MMIO stale data needs all three of SBDR_SSDP_NO, FBSDP_NO, PSDP_NO.
    CHECK_EQ(cpu_immunities(false, &r, (1ull << 13) | (1ull << 14), n, 24), 0);
    k = cpu_immunities(false, &r, (1ull << 13) | (1ull << 14) | (1ull << 15), n, 24);
    CHECK_EQ(k, 1);
    CHECK_STR(n[0], "MMIO stale data");
    // AMD: immune by design to several, plus SSB_NO / SRSO_NO from CPUID.
    r.e8_ebx = 1u << 26;
    r.e21_eax = 1u << 29;
    k = cpu_immunities(true, &r, 0, n, 24);
    CHECK(find(n, k, "Meltdown") >= 0);
    CHECK(find(n, k, "L1TF") >= 0);
    CHECK(find(n, k, "MDS") >= 0);
    CHECK(find(n, k, "Spectre v4 (SSB)") >= 0);
    CHECK(find(n, k, "SRSO (Inception)") >= 0);
    CHECK(find(n, k, "Downfall (GDS)") >= 0);  // Intel-only bug
    r.e21_eax = 0;
    CHECK(find(n, cpu_immunities(true, &r, 0, n, 24), "SRSO (Inception)") < 0);  // only with SRSO_NO
    CHECK_EQ(cpu_immunities(true, &r, 0, n, 2), 2);  // max respected

    // Mitigation support.
    memset(&r, 0, sizeof r);
    r.l7_edx = (1u << 26) | (1u << 27) | (1u << 31) | (1u << 10) | (1u << 28);
    k = cpu_mitigations(&r, 1ull << 1, n, 24);
    CHECK_EQ(k, 6);
    CHECK_STR(n[0], "IBRS/IBPB");
    CHECK_STR(n[1], "STIBP");
    CHECK_STR(n[2], "SSBD");
    CHECK_STR(n[3], "MD_CLEAR");
    CHECK_STR(n[4], "L1D flush");
    CHECK_STR(n[5], "enhanced IBRS");
    memset(&r, 0, sizeof r);
    r.e8_ebx = (1u << 12) | (1u << 14) | (1u << 15) | (1u << 24);
    r.e21_eax = 1u << 8;
    r.l7_edx = 1u << 27;  // STIBP from both leaves: listed once
    k = cpu_mitigations(&r, 0, n, 24);
    CHECK_EQ(k, 5);
    CHECK(find(n, k, "STIBP") >= 0);
    CHECK(find(n, k, "IBPB") >= 0);
    CHECK(find(n, k, "IBRS") >= 0);
    CHECK(find(n, k, "SSBD") >= 0);
    CHECK(find(n, k, "automatic IBRS") >= 0);
}

int main(void) {
    test_security();
    test_amd_legacy_cache();
    test_signature();
    test_features();
    test_cache();
    TEST_MAIN_END();
}
