#include "info/cpu.h"
#include "lib/fmt.h"
#include "ui/tabs.h"

static const char *cache_type_name(enum cache_type t) {
    switch (t) {
    case CACHE_DATA: return "data";
    case CACHE_INSN: return "instruction";
    case CACHE_UNIFIED: return "unified";
    default: return "unknown";
    }
}

static const char *tsc_source_name(enum tsc_source s) {
    switch (s) {
    case TSC_SRC_LIMINE: return "from Limine";
    case TSC_SRC_CPUID: return "from CPUID";
    case TSC_SRC_CPUID_BASE: return "CPUID base clock";
    case TSC_SRC_PIT: return "PIT calibrated";
    default: return "unknown";
    }
}

void tab_cpu_build(struct page *p, const struct cpu_info *c, size_t width) {
    page_clear(p);
    p->title = "CPU";

    page_add(p, STYLE_HEAD, "Processor");
    page_kv(p, "Vendor", "%s", c->vendor[0] ? c->vendor : "-");
    page_kv(p, "Brand", "%s", c->brand[0] ? c->brand : "-");
    page_kv(p, "Signature", "family %u, model %u (0x%x), stepping %u",
            c->sig.family, c->sig.model, c->sig.model, c->sig.stepping);
    if (c->hypervisor[0])
        page_kv(p, "Hypervisor", "%s", c->hypervisor);
    if (c->cpu_count)
        page_kv(p, "Logical CPUs", "%lu (BSP LAPIC ID %u)", (unsigned long)c->cpu_count, c->bsp_lapic_id);
    else
        page_kv(p, "Logical CPUs", "unknown (no MP response)");
    if (c->tsc_hz)
        page_kv(p, "TSC", "%s%lu.%03lu MHz (%s)", (c->tsc_source == TSC_SRC_PIT || c->tsc_source == TSC_SRC_CPUID_BASE) ? "~" : "",
                (unsigned long)(c->tsc_hz / 1000000), (unsigned long)(c->tsc_hz / 1000 % 1000),
                tsc_source_name(c->tsc_source));
    else
        page_kv(p, "TSC", "unknown");
    page_kv(p, "CPUID", "max leaf 0x%x, max extended 0x%x", c->max_leaf, c->max_ext_leaf);

    page_blank(p);
    page_add(p, STYLE_HEAD, "Security");
    if (c->have_microcode)
        page_kv(p, "Microcode", "0x%x", c->microcode);
    else
        page_kv(p, "Microcode", "not readable%s", c->hypervisor[0] ? " (virtual machine)" : "");
    if (c->immune_count)
        page_wrap_list(p, "Not affected", c->immune, c->immune_count, width);
    else
        page_kv(p, "Not affected", "none claimed by the CPU");
    if (c->mitigation_count)
        page_wrap_list(p, "Mitigations", c->mitigations, c->mitigation_count, width);
    else
        page_kv(p, "Mitigations", "no speculation controls reported");
    page_add(p, STYLE_DIM, "Issues not listed as \"not affected\" may apply; an OS has to mitigate them.");

    page_blank(p);
    page_add(p, STYLE_HEAD, "Caches");
    if (!c->cache_count)
        page_add(p, STYLE_DIM, "Not reported by CPUID");
    for (size_t i = 0; i < c->cache_count; i++) {
        char key[24], size[24];
        ksnprintf(key, sizeof key, "L%u %s", c->caches[i].level, cache_type_name(c->caches[i].type));
        fmt_size(size, sizeof size, c->caches[i].size);
        page_kv(p, key, "%s", size);
    }

    page_blank(p);
    page_add(p, STYLE_HEAD, "Features (%zu)", c->feature_count);
    page_wrap_words(p, "", c->features, c->feature_count, width);
}
