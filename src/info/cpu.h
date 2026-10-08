#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "info/tsc.h"

struct cpu_sig {
    unsigned family, model, stepping;
};

// Feature registers from CPUID leaves 1, 7.0 and 0x80000001 (0 when absent).
struct cpu_feature_regs {
    uint32_t l1_ecx, l1_edx, l7_ebx, l7_ecx, l7_edx, e1_ecx, e1_edx;
};

enum cache_type { CACHE_NONE = 0, CACHE_DATA = 1, CACHE_INSN = 2, CACHE_UNIFIED = 3 };

struct cache_desc {
    unsigned level;
    enum cache_type type;
    uint64_t size;  // bytes
};

#define CPU_MAX_CACHES 8
#define CPU_MAX_FEATURES 96

struct cpu_info {
    char vendor[13];
    char brand[49];
    char hypervisor[13];  // empty when not virtualised
    struct cpu_sig sig;
    uint32_t max_leaf, max_ext_leaf;
    struct cpu_feature_regs regs;
    size_t feature_count;
    const char *features[CPU_MAX_FEATURES];
    size_t cache_count;
    struct cache_desc caches[CPU_MAX_CACHES];
    uint64_t cpu_count;  // 0 when unknown
    uint32_t bsp_lapic_id;
    uint64_t tsc_hz;     // 0 when unknown
    enum tsc_source tsc_source;
};

// Pure decoders (cpu_decode.c).
void cpu_decode_signature(uint32_t leaf1_eax, struct cpu_sig *out);
size_t cpu_feature_names(const struct cpu_feature_regs *r, const char **names, size_t max);
// Decodes one CPUID leaf 4 / 0x8000001D subleaf; false for the terminating entry.
bool cpu_decode_cache(uint32_t eax, uint32_t ebx, uint32_t ecx, struct cache_desc *out);

// Pre-Zen AMD fallback: L1d/L1i from leaf 0x80000005 ECX/EDX, L2/L3 from 0x80000006
// ECX/EDX. Zero-sized caches are skipped. Returns the number written (<= max).
size_t cpu_decode_amd_legacy_caches(uint32_t l5_ecx, uint32_t l5_edx, uint32_t l6_ecx, uint32_t l6_edx,
                                    struct cache_desc *out, size_t max);

// Hardware probe (cpu.c). Call after tsc_init().
void cpu_probe(struct cpu_info *out);
