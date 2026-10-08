#include "info/mem.h"
#include <stdbool.h>
#include "lib/string.h"

static bool is_available(uint64_t type) {
    return type == LIMINE_MEMMAP_USABLE || type == LIMINE_MEMMAP_BOOTLOADER_RECLAIMABLE ||
           type == LIMINE_MEMMAP_EXECUTABLE_AND_MODULES || type == LIMINE_MEMMAP_ACPI_RECLAIMABLE;
}

void mem_summarize(struct limine_memmap_entry *const *entries, uint64_t count, struct mem_summary *out) {
    memset(out, 0, sizeof *out);
    out->entries = (size_t)count;
    for (uint64_t i = 0; i < count; i++) {
        const struct limine_memmap_entry *e = entries[i];
        if (e->type < MEM_TYPES)
            out->by_type[e->type] += e->length;
        if (is_available(e->type)) {
            out->available += e->length;
            if (e->base + e->length > out->highest)
                out->highest = e->base + e->length;
        }
    }
}

const char *mem_type_name(uint64_t type) {
    static const char *const names[MEM_TYPES] = {
        "usable", "reserved", "ACPI reclaimable", "ACPI NVS", "bad memory",
        "bootloader reclaimable", "kernel and modules", "framebuffer", "reserved (mapped)",
    };
    return type < MEM_TYPES ? names[type] : "unknown";
}
