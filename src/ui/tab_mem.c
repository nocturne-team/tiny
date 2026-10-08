#include "info/mem.h"
#include "lib/fmt.h"
#include "ui/tabs.h"

static void size_kv(struct page *p, const char *key, uint64_t bytes, const char *note) {
    char buf[24];
    fmt_size(buf, sizeof buf, bytes);
    page_kv(p, key, "%s%s", buf, note);
}

void tab_mem_build(struct page *p, const struct limine_memmap_response *map) {
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
