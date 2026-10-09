#include "info/disk.h"
#include "lib/fmt.h"
#include "ui/tabs.h"

// Decimal units with one truncated decimal, as drives are sold: "1.0 TB".
static void dec_size(char *buf, size_t cap, uint64_t bytes) {
    static const char *const units[] = { "B", "kB", "MB", "GB", "TB", "PB", "EB" };
    size_t u = 0;
    uint64_t div = 1;
    while (u + 1 < sizeof units / sizeof *units && bytes / div >= 1000) {
        div *= 1000;
        u++;
    }
    if (!u) {
        ksnprintf(buf, cap, "%llu B", (unsigned long long)bytes);
        return;
    }
    uint64_t whole = bytes / div, tenth = bytes % div / (div / 10);
    ksnprintf(buf, cap, "%llu.%llu %s", (unsigned long long)whole, (unsigned long long)tenth, units[u]);
}

static const char *plural(uint64_t n) {
    return n == 1 ? "" : "s";
}

static void health_line(struct page *p, const struct disk_info *d) {
    enum disk_health h = disk_health(d);
    const char *name = disk_health_name(h);
    char why[80] = "";
    if (d->bus == DISK_NVME && h != HEALTH_OK && h != HEALTH_UNKNOWN && d->crit_warning)
        ksnprintf(why, sizeof why, " (critical warning 0x%02x)", d->crit_warning);
    else if (d->bus == DISK_NVME && h == HEALTH_FAILING)
        ksnprintf(why, sizeof why, " (spare %u%% below threshold %u%%)", d->spare, d->spare_thresh);
    else if (d->bus == DISK_NVME && h == HEALTH_WORN)
        ksnprintf(why, sizeof why, " (%u%% of rated life used)", d->used_pct);
    else if (h == HEALTH_FAILING)
        ksnprintf(why, sizeof why, " (SMART threshold exceeded)");
    else if (h == HEALTH_WARNING)
        ksnprintf(why, sizeof why, " (%llu reallocated, %llu pending, %llu uncorrectable sectors)",
                  (unsigned long long)(d->realloc == DISK_NONE ? 0 : d->realloc),
                  (unsigned long long)(d->pending == DISK_NONE ? 0 : d->pending),
                  (unsigned long long)(d->uncorrectable == DISK_NONE ? 0 : d->uncorrectable));
    page_kv(p, "Health", "%s%s", name, why);
    if (h == HEALTH_WARNING || h == HEALTH_WORN || h == HEALTH_FAILING)
        p->lines[p->count - 1].style = STYLE_WARN;
}

// "<v> <one|many>", or the bare number when `one` is NULL; nothing when absent.
static void counter(struct page *p, const char *key, uint64_t v, const char *one, const char *many) {
    if (v == DISK_NONE)
        return;
    if (one)
        page_kv(p, key, "%llu %s", (unsigned long long)v, v == 1 ? one : many);
    else
        page_kv(p, key, "%llu", (unsigned long long)v);
}

static void disk(struct page *p, const struct disk_info *d) {
    char size[24];
    page_blank(p);
    if (d->bus == DISK_NVME) {
        page_add(p, STYLE_HEAD, "NVMe  %s", d->model);
        page_kv(p, "Interface", "NVMe at %02x:%02x.%x, %u namespace%s", d->pci_bus, d->pci_dev,
                d->pci_fn, d->namespaces, plural(d->namespaces));
    } else {
        page_add(p, STYLE_HEAD, "%s  %s", d->bus == DISK_ATAPI ? "Optical" : "SATA", d->model);
        const char *kind = d->rpm == 1 ? ", SSD" : "";
        char rpm[16] = "";
        if (d->rpm > 1)
            ksnprintf(rpm, sizeof rpm, ", %u rpm", d->rpm);
        page_kv(p, "Interface", "SATA (AHCI %02x:%02x.%x, port %u)%s%s", d->pci_bus, d->pci_dev,
                d->pci_fn, d->port, kind, rpm);
    }
    if (d->bytes) {
        dec_size(size, sizeof size, d->bytes);
        page_kv(p, "Capacity", "%s (%u-byte sectors)", size, d->sector);
    }
    if (d->serial[0])
        page_kv(p, "Serial", "%s", d->serial);
    if (d->firmware[0])
        page_kv(p, "Firmware", "%s", d->firmware);
    if (d->bus == DISK_ATAPI)
        return;

    if (d->bus == DISK_SATA && !d->smart_supported)
        page_kv(p, "SMART", "not supported");
    else if (d->bus == DISK_SATA && !d->smart_enabled)
        page_kv(p, "SMART", "supported but disabled");
    health_line(p, d);
    if (d->has_temp)
        page_kv(p, "Temperature", "%d \xf8" "C", d->temp_c);
    if (d->power_on_hours != DISK_NONE && d->power_cycles != DISK_NONE)
        page_kv(p, "Power-on", "%llu hour%s, %llu power cycle%s",
                (unsigned long long)d->power_on_hours, plural(d->power_on_hours),
                (unsigned long long)d->power_cycles, plural(d->power_cycles));
    else if (d->power_on_hours != DISK_NONE)
        page_kv(p, "Power-on", "%llu hour%s", (unsigned long long)d->power_on_hours,
                plural(d->power_on_hours));
    else
        counter(p, "Power cycles", d->power_cycles, NULL, NULL);

    if (d->bus == DISK_NVME && d->smart) {
        page_kv(p, "Life used", "%u%% (spare %u%%, threshold %u%%)", d->used_pct, d->spare,
                d->spare_thresh);
        char rd[24], wr[24];
        dec_size(rd, sizeof rd, d->bytes_read);
        dec_size(wr, sizeof wr, d->bytes_written);
        page_kv(p, "Data", "%s read, %s written", rd, wr);
        counter(p, "Unsafe shutdowns", d->unsafe_shutdowns, NULL, NULL);
        counter(p, "Media errors", d->media_errors, NULL, NULL);
        counter(p, "Error log", d->error_entries, "entry", "entries");
    } else if (d->smart) {
        counter(p, "Reallocated", d->realloc, "sector", "sectors");
        counter(p, "Pending", d->pending, "sector", "sectors");
        counter(p, "Uncorrectable", d->uncorrectable, "sector", "sectors");
    }
}

void tab_storage_build(struct page *p, const struct storage_list *s) {
    page_clear(p);
    p->title = "Storage";
    if (s->disabled) {
        page_add(p, STYLE_WARN, "Storage probing turned off (nostorage on the command line)");
        return;
    }
    if (!s->probed) {
        page_add(p, STYLE_WARN, "Storage not probed (no DMA memory)");
        return;
    }
    if (s->count)
        page_add(p, STYLE_HEAD, "Drives (%zu)", s->count);
    else
        page_add(p, STYLE_WARN, "No drives found");
    for (size_t i = 0; i < s->count; i++)
        disk(p, &s->disks[i]);

    if (s->raid_ctrls || s->ide_ctrls)
        page_blank(p);
    if (s->raid_ctrls) {
        page_add(p, STYLE_WARN, "%zu RAID/VMD controller%s: drives behind %s are not shown.",
                 s->raid_ctrls, plural(s->raid_ctrls), s->raid_ctrls == 1 ? "it" : "them");
        page_add(p, STYLE_DIM, "Switch the firmware's storage mode to AHCI to see them.");
    }
    if (s->ide_ctrls)
        page_add(p, STYLE_DIM, "%zu legacy IDE controller%s: not supported", s->ide_ctrls,
                 plural(s->ide_ctrls));
}
