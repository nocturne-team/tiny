#include "info/fw.h"
#include "boot.h"
#include "gfx/gfx.h"
#include "info/rtc.h"
#include "lib/fmt.h"
#include "lib/serial.h"
#include "lib/string.h"

#define MAX_RANGES 256

static struct phys_range ranges[MAX_RANGES];

// Memory-map types Limine maps in the HHDM at base revision 4 and later.
static bool hhdm_mapped(uint64_t type) {
    switch (type) {
    case LIMINE_MEMMAP_USABLE:
    case LIMINE_MEMMAP_BOOTLOADER_RECLAIMABLE:
    case LIMINE_MEMMAP_EXECUTABLE_AND_MODULES:
    case LIMINE_MEMMAP_FRAMEBUFFER:
    case LIMINE_MEMMAP_ACPI_RECLAIMABLE:
    case LIMINE_MEMMAP_ACPI_NVS:
    case LIMINE_MEMMAP_RESERVED_MAPPED:
        return true;
    default:
        return false;
    }
}

// Ranges firmware tables may be read from; NULL (trust Limine) without a memory map.
static const struct phys_ranges *mapped_ranges(struct phys_ranges *out) {
    const struct limine_memmap_response *map = bootinfo.memmap;
    if (!map)
        return NULL;
    out->r = ranges;
    out->n = 0;
    for (uint64_t i = 0; i < map->entry_count && out->n < MAX_RANGES; i++)
        if (hhdm_mapped(map->entries[i]->type))
            ranges[out->n++] = (struct phys_range){ map->entries[i]->base, map->entries[i]->length };
    return out;
}

void fw_probe(struct fw_info *f) {
    memset(f, 0, sizeof *f);
    f->firmware_type = bootinfo.firmware;
    if (bootinfo.bl_name)
        ksnprintf(f->bootloader, sizeof f->bootloader, "%s %s", bootinfo.bl_name,
                  bootinfo.bl_version ? bootinfo.bl_version : "");
    if (bootinfo.have_boot_time) {
        f->have_boot_time = true;
        unix_to_datetime(bootinfo.boot_unix, &f->boot_time);
    }

    if (bootinfo.fb) {
        f->fb_width = bootinfo.fb->width;
        f->fb_height = bootinfo.fb->height;
        f->fb_pitch = bootinfo.fb->pitch;
        f->fb_bpp = bootinfo.fb->bpp;
    }
    const struct gfx_layout *l = gfx_get_layout();
    f->ui_cols = l->cols;
    f->ui_rows = l->rows;
    f->ui_scale = l->scale;

    struct datetime now;
    uint8_t reg_b = 0;
    f->rtc_ok = rtc_read(&now, &reg_b);
    f->rtc_bcd = !(reg_b & 0x04);
    f->rtc_24h = reg_b & 0x02;

    // Table addresses are physical: without an HHDM offset they cannot be read.
    if (bootinfo.hhdm) {
        struct phys_ranges storage;
        const struct phys_ranges *mapped = mapped_ranges(&storage);
        acpi_parse(bootinfo.rsdp, bootinfo.hhdm, mapped, &f->acpi);
        smbios_parse(bootinfo.smbios32, bootinfo.smbios64, bootinfo.hhdm, mapped, &f->smbios);
    }

    if (f->acpi.present)
        klog("acpi: rev %u, %zu tables via %s", f->acpi.revision, f->acpi.table_count,
             f->acpi.use_xsdt ? "XSDT" : "RSDT");
    else
        klog("acpi: not provided");
    if (f->smbios.present)
        klog("smbios: %s %s (SMBIOS %u.%u)", f->smbios.sys_vendor, f->smbios.sys_product,
             f->smbios.major, f->smbios.minor);
    else
        klog("smbios: not provided");
    klog("smbios: %zu memory devices", f->smbios.dimm_count);
}
