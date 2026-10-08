#include "info/cpu.h"
#include "arch/x86.h"
#include "boot.h"
#include "lib/string.h"

static void copy_regs(char *dst, uint32_t a, uint32_t b, uint32_t c) {
    memcpy(dst + 0, &a, 4);
    memcpy(dst + 4, &b, 4);
    memcpy(dst + 8, &c, 4);
}

static void trim_leading_spaces(char *s) {
    size_t i = 0;
    while (s[i] == ' ')
        i++;
    if (i)
        memmove(s, s + i, strlen(s + i) + 1);
}

void cpu_probe(struct cpu_info *c) {
    memset(c, 0, sizeof *c);

    struct cpuid_regs r = cpuid(0, 0);
    c->max_leaf = r.eax;
    copy_regs(c->vendor, r.ebx, r.edx, r.ecx);
    c->max_ext_leaf = cpuid(0x80000000, 0).eax;
    if (c->max_ext_leaf < 0x80000000)
        c->max_ext_leaf = 0;

    if (c->max_leaf >= 1) {
        r = cpuid(1, 0);
        cpu_decode_signature(r.eax, &c->sig);
        c->regs.l1_ecx = r.ecx;
        c->regs.l1_edx = r.edx;
    }
    if (c->max_leaf >= 7) {
        r = cpuid(7, 0);
        c->regs.l7_ebx = r.ebx;
        c->regs.l7_ecx = r.ecx;
        c->regs.l7_edx = r.edx;
    }
    if (c->max_ext_leaf >= 0x80000001) {
        r = cpuid(0x80000001, 0);
        c->regs.e1_ecx = r.ecx;
        c->regs.e1_edx = r.edx;
    }
    if (c->max_ext_leaf >= 0x80000004) {
        for (uint32_t i = 0; i < 3; i++) {
            r = cpuid(0x80000002 + i, 0);
            memcpy(c->brand + i * 16, &r, 16);
        }
        trim_leading_spaces(c->brand);
    }
    if (c->regs.l1_ecx & (1u << 31)) {
        r = cpuid(0x40000000, 0);
        copy_regs(c->hypervisor, r.ebx, r.ecx, r.edx);
    }
    c->feature_count = cpu_feature_names(&c->regs, c->features, CPU_MAX_FEATURES);

    bool amd = memcmp(c->vendor, "AuthenticAMD", 12) == 0 || memcmp(c->vendor, "HygonGenuine", 12) == 0;
    uint32_t leaf = 0;
    if (amd && (c->regs.e1_ecx & (1u << 22)) && c->max_ext_leaf >= 0x8000001D)
        leaf = 0x8000001D;
    else if (c->max_leaf >= 4)
        leaf = 4;
    for (uint32_t i = 0; leaf && i < CPU_MAX_CACHES; i++) {
        r = cpuid(leaf, i);
        if (!cpu_decode_cache(r.eax, r.ebx, r.ecx, &c->caches[c->cache_count]))
            break;
        c->cache_count++;
    }

    if (!c->cache_count && amd && c->max_ext_leaf >= 0x80000006) {
        struct cpuid_regs l5 = cpuid(0x80000005, 0), l6 = cpuid(0x80000006, 0);
        c->cache_count = cpu_decode_amd_legacy_caches(l5.ecx, l5.edx, l6.ecx, l6.edx, c->caches, CPU_MAX_CACHES);
    }

    c->cpu_count = bootinfo.have_mp ? bootinfo.cpu_count : 0;
    c->bsp_lapic_id = bootinfo.bsp_lapic_id;
    c->tsc_hz = tsc_hz();
    c->tsc_source = tsc_source();
}
