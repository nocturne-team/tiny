#include "info/cpu.h"

void cpu_decode_signature(uint32_t eax, struct cpu_sig *out) {
    unsigned base_family = (eax >> 8) & 0xF;
    unsigned base_model = (eax >> 4) & 0xF;
    out->stepping = eax & 0xF;
    out->family = base_family == 0xF ? base_family + ((eax >> 20) & 0xFF) : base_family;
    out->model = (base_family == 0x6 || base_family == 0xF)
                     ? base_model + (((eax >> 16) & 0xF) << 4)
                     : base_model;
}

enum { L1C, L1D, L7B, L7C, L7D, E1C, E1D };

static const struct {
    uint8_t reg, bit;
    const char *name;
} feature_table[] = {
    { L1D, 0, "fpu" }, { L1D, 4, "tsc" }, { L1D, 5, "msr" }, { L1D, 6, "pae" },
    { L1D, 9, "apic" }, { L1D, 12, "mtrr" }, { L1D, 13, "pge" }, { L1D, 15, "cmov" },
    { L1D, 16, "pat" }, { L1D, 19, "clflush" }, { L1D, 23, "mmx" }, { L1D, 24, "fxsr" },
    { L1D, 25, "sse" }, { L1D, 26, "sse2" }, { L1D, 28, "htt" },
    { L1C, 0, "sse3" }, { L1C, 1, "pclmulqdq" }, { L1C, 3, "monitor" }, { L1C, 5, "vmx" },
    { L1C, 9, "ssse3" }, { L1C, 12, "fma" }, { L1C, 13, "cx16" }, { L1C, 19, "sse4.1" },
    { L1C, 20, "sse4.2" }, { L1C, 21, "x2apic" }, { L1C, 22, "movbe" }, { L1C, 23, "popcnt" },
    { L1C, 24, "tsc-deadline" }, { L1C, 25, "aes" }, { L1C, 26, "xsave" }, { L1C, 28, "avx" },
    { L1C, 29, "f16c" }, { L1C, 30, "rdrand" }, { L1C, 31, "hypervisor" },
    { L7B, 0, "fsgsbase" }, { L7B, 3, "bmi1" }, { L7B, 5, "avx2" }, { L7B, 7, "smep" },
    { L7B, 8, "bmi2" }, { L7B, 9, "erms" }, { L7B, 10, "invpcid" }, { L7B, 16, "avx512f" },
    { L7B, 17, "avx512dq" }, { L7B, 18, "rdseed" }, { L7B, 19, "adx" }, { L7B, 20, "smap" },
    { L7B, 23, "clflushopt" }, { L7B, 24, "clwb" }, { L7B, 28, "avx512cd" }, { L7B, 29, "sha" },
    { L7B, 30, "avx512bw" }, { L7B, 31, "avx512vl" },
    { L7C, 1, "avx512vbmi" }, { L7C, 2, "umip" }, { L7C, 3, "pku" }, { L7C, 8, "gfni" },
    { L7C, 9, "vaes" }, { L7C, 10, "vpclmulqdq" }, { L7C, 11, "avx512vnni" }, { L7C, 22, "rdpid" },
    { L7D, 14, "serialize" }, { L7D, 15, "hybrid" },
    { E1C, 0, "lahf_lm" }, { E1C, 2, "svm" }, { E1C, 5, "lzcnt" }, { E1C, 6, "sse4a" },
    { E1C, 8, "prefetchw" }, { E1C, 11, "xop" }, { E1C, 16, "fma4" }, { E1C, 22, "topoext" },
    { E1D, 11, "syscall" }, { E1D, 20, "nx" }, { E1D, 26, "pdpe1gb" }, { E1D, 27, "rdtscp" },
    { E1D, 29, "lm" },
};

static uint32_t reg_value(const struct cpu_feature_regs *r, int reg) {
    switch (reg) {
    case L1C: return r->l1_ecx;
    case L1D: return r->l1_edx;
    case L7B: return r->l7_ebx;
    case L7C: return r->l7_ecx;
    case L7D: return r->l7_edx;
    case E1C: return r->e1_ecx;
    default:  return r->e1_edx;
    }
}

size_t cpu_feature_names(const struct cpu_feature_regs *r, const char **names, size_t max) {
    size_t n = 0;
    for (size_t i = 0; i < sizeof feature_table / sizeof feature_table[0] && n < max; i++)
        if (reg_value(r, feature_table[i].reg) & (1u << feature_table[i].bit))
            names[n++] = feature_table[i].name;
    return n;
}

bool cpu_decode_cache(uint32_t eax, uint32_t ebx, uint32_t ecx, struct cache_desc *out) {
    unsigned type = eax & 0x1F;
    if (type == 0)
        return false;
    uint64_t line = (ebx & 0xFFF) + 1;
    uint64_t parts = ((ebx >> 12) & 0x3FF) + 1;
    uint64_t ways = ((ebx >> 22) & 0x3FF) + 1;
    uint64_t sets = (uint64_t)ecx + 1;
    out->level = (eax >> 5) & 0x7;
    out->type = type <= 3 ? (enum cache_type)type : CACHE_UNIFIED;
    out->size = line * parts * ways * sets;
    return true;
}

size_t cpu_decode_amd_legacy_caches(uint32_t l5_ecx, uint32_t l5_edx, uint32_t l6_ecx, uint32_t l6_edx,
                                    struct cache_desc *out, size_t max) {
    const struct cache_desc all[4] = {
        { 1, CACHE_DATA, (uint64_t)(l5_ecx >> 24) << 10 },
        { 1, CACHE_INSN, (uint64_t)(l5_edx >> 24) << 10 },
        { 2, CACHE_UNIFIED, (uint64_t)(l6_ecx >> 16) << 10 },
        { 3, CACHE_UNIFIED, (uint64_t)(l6_edx >> 18) << 19 },
    };
    size_t n = 0;
    for (size_t i = 0; i < 4 && n < max; i++)
        if (all[i].size)
            out[n++] = all[i];
    return n;
}
