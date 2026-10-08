#include "info/mem.h"
#include "info/smbios.h"
#include "lib/fmt.h"
#include "ui/tabs.h"

static void size_kv(struct page *p, const char *key, uint64_t bytes, const char *note) {
    char buf[24];
    fmt_size(buf, sizeof buf, bytes);
    page_kv(p, key, "%s%s", buf, note);
}

static void dimm_section(struct page *p, const struct smbios_info *smb) {
    size_t populated = 0;
    uint64_t total = 0;
    for (size_t i = 0; i < smb->dimm_count; i++) {
        populated += smb->dimms[i].size != 0 || smb->dimms[i].size_unknown;
        total += smb->dimms[i].size;
    }

    page_blank(p);
    page_add(p, STYLE_HEAD, "Installed modules (%zu of %zu slots)", populated, smb->dimm_count);
    size_kv(p, "Total installed", total, smb->dimms_truncated ? " (list truncated)" : "");
    for (size_t i = 0; i < smb->dimm_count; i++) {
        const struct smbios_dimm *d = &smb->dimms[i];
        const char *loc = d->locator[0] ? d->locator : "-";
        if (!d->size && !d->size_unknown) {
            page_add(p, STYLE_DIM, "%-14.14s %10s", loc, "empty");
            continue;
        }
        char size[24], speed[16];
        if (d->size_unknown)
            ksnprintf(size, sizeof size, "unknown");
        else
            fmt_size(size, sizeof size, d->size);
        if (d->speed_mts)
            ksnprintf(speed, sizeof speed, "%u MT/s", d->speed_mts);
        else
            ksnprintf(speed, sizeof speed, "-");
        page_add(p, STYLE_TEXT, "%-14.14s %10s  %-6s  %9s  %s%s%s", loc, size,
                 smbios_memory_type_name(d->type), speed, d->manufacturer,
                 d->manufacturer[0] && d->part[0] ? " " : "", d->part);
    }
}

void tab_mem_build(struct page *p, const struct limine_memmap_response *map, const struct smbios_info *smb) {
    page_clear(p);
    p->title = "Memory";
    if (!map) {
        page_add(p, STYLE_WARN, "Memory map not provided by bootloader");
        return;
    }

    struct mem_summary s;
    mem_summarize(map->entries, map->entry_count, &s);

    page_add(p, STYLE_HEAD, "Summary");
    size_kv(p, "Available", s.available, "");
    size_kv(p, "Usable", s.by_type[LIMINE_MEMMAP_USABLE], "");
    size_kv(p, "Bootloader", s.by_type[LIMINE_MEMMAP_BOOTLOADER_RECLAIMABLE], " (reclaimable)");
    size_kv(p, "ACPI reclaim.", s.by_type[LIMINE_MEMMAP_ACPI_RECLAIMABLE], "");
    size_kv(p, "ACPI NVS", s.by_type[LIMINE_MEMMAP_ACPI_NVS], "");
    size_kv(p, "Reserved", s.by_type[LIMINE_MEMMAP_RESERVED] + s.by_type[LIMINE_MEMMAP_RESERVED_MAPPED], "");
    size_kv(p, "Framebuffer", s.by_type[LIMINE_MEMMAP_FRAMEBUFFER], "");
    size_kv(p, "Kernel", s.by_type[LIMINE_MEMMAP_EXECUTABLE_AND_MODULES], "");
    if (s.by_type[LIMINE_MEMMAP_BAD_MEMORY])
        size_kv(p, "Bad memory", s.by_type[LIMINE_MEMMAP_BAD_MEMORY], "");
    page_kv(p, "Highest addr", "0x%lx", (unsigned long)s.highest);
    if (smb && smb->dimm_count)
        dimm_section(p, smb);

    page_blank(p);
    page_add(p, STYLE_HEAD, "Memory map (%zu entries)", s.entries);
    page_add(p, STYLE_DIM, "%-16s  %-16s  %10s  %s", "Base", "End (incl.)", "Size", "Type");
    for (uint64_t i = 0; i < map->entry_count && !p->truncated; i++) {
        const struct limine_memmap_entry *e = map->entries[i];
        char size[24];
        fmt_size(size, sizeof size, e->length);
        uint64_t end = e->length ? e->base + e->length - 1 : e->base;
        page_add(p, STYLE_TEXT, "%016lx  %016lx  %10s  %s", (unsigned long)e->base,
                 (unsigned long)end, size, mem_type_name(e->type));
    }
}
