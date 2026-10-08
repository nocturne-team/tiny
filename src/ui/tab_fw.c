#include "info/fw.h"
#include "lib/fmt.h"
#include "ui/tabs.h"

static const char *dash(const char *s) {
    return s[0] ? s : "-";
}

static const char *firmware_name(int type) {
    switch (type) {
    case LIMINE_FIRMWARE_TYPE_X86BIOS: return "BIOS";
    case LIMINE_FIRMWARE_TYPE_EFI32: return "UEFI (32-bit)";
    case LIMINE_FIRMWARE_TYPE_EFI64: return "UEFI (64-bit)";
    default: return "unknown";
    }
}

void tab_fw_build(struct page *p, const struct fw_info *f) {
    page_clear(p);
    p->title = "Firmware";

    page_add(p, STYLE_HEAD, "Boot");
    page_kv(p, "Firmware", "%s", firmware_name(f->firmware_type));
    page_kv(p, "Bootloader", "%s", dash(f->bootloader));
    if (f->have_boot_time)
        page_kv(p, "Boot time", "%04d-%02d-%02d %02d:%02d:%02d UTC", f->boot_time.year,
                f->boot_time.month, f->boot_time.day, f->boot_time.hour, f->boot_time.min,
                f->boot_time.sec);
    else
        page_kv(p, "Boot time", "not provided by bootloader");
    page_kv(p, "Display", "%lux%lu, %u bpp, pitch %lu", (unsigned long)f->fb_width,
            (unsigned long)f->fb_height, f->fb_bpp, (unsigned long)f->fb_pitch);
    page_kv(p, "Text grid", "%dx%d cells, scale %d", f->ui_cols, f->ui_rows, f->ui_scale);
    if (f->rtc_ok)
        page_kv(p, "RTC", "%s, %s", f->rtc_bcd ? "BCD" : "binary", f->rtc_24h ? "24-hour" : "12-hour");
    else
        page_kv(p, "RTC", "not responding");

    page_blank(p);
    page_add(p, STYLE_HEAD, "Monitor");
    const struct edid_info *e = &f->edid;
    if (!f->have_edid) {
        page_add(p, STYLE_WARN, "No EDID from the firmware");
    } else if (!e->valid) {
        page_add(p, STYLE_WARN, "EDID present but invalid");
    } else {
        page_kv(p, "Name", "%s", dash(e->name));
        page_kv(p, "Manufacturer", "%s, product 0x%04x", e->manufacturer, e->product);
        if (e->model_year)
            page_kv(p, "Manufactured", "model year %d", e->year);
        else if (e->week)
            page_kv(p, "Manufactured", "week %d of %d", e->week, e->year);
        else
            page_kv(p, "Manufactured", "%d", e->year);
        if (e->diagonal_tenths)
            page_kv(p, "Size", "%u x %u cm (%u.%u\")", e->width_cm, e->height_cm,
                    e->diagonal_tenths / 10, e->diagonal_tenths % 10);
        if (e->pref_width)
            page_kv(p, "Preferred mode", "%ux%u @ %u.%02u Hz", e->pref_width, e->pref_height,
                    e->pref_centihz / 100, e->pref_centihz % 100);
        page_kv(p, "EDID", "version %u.%u%s", e->version, e->revision, e->checksum_ok ? "" : " (bad csum)");
    }

    page_blank(p);
    page_add(p, STYLE_HEAD, "SMBIOS");
    const struct smbios_info *s = &f->smbios;
    if (!s->present) {
        page_add(p, STYLE_WARN, "Not provided by bootloader");
    } else {
        page_kv(p, "Version", "%u.%u (%s entry point)%s", s->major, s->minor,
                s->is64 ? "64-bit" : "32-bit", s->checksum_ok ? "" : " (bad csum)");
        page_kv(p, "BIOS vendor", "%s", dash(s->bios_vendor));
        page_kv(p, "BIOS version", "%s", dash(s->bios_version));
        page_kv(p, "BIOS date", "%s", dash(s->bios_date));
        page_kv(p, "System", "%s %s", dash(s->sys_vendor), dash(s->sys_product));
        page_kv(p, "System version", "%s", dash(s->sys_version));
        page_kv(p, "Board", "%s %s", dash(s->board_vendor), dash(s->board_product));
    }

    page_blank(p);
    page_add(p, STYLE_HEAD, "ACPI");
    const struct acpi_info *a = &f->acpi;
    if (!a->present) {
        page_add(p, STYLE_WARN, "Not provided by bootloader");
        return;
    }
    page_kv(p, "RSDP", "revision %u (%s), OEM \"%s\"%s", a->revision,
            a->revision >= 2 ? "ACPI 2.0+" : "ACPI 1.0", a->oem_id,
            a->rsdp_checksum_ok ? "" : " (bad csum)");
    page_kv(p, "Tables", "%zu via %s%s", a->table_count, a->use_xsdt ? "XSDT" : "RSDT",
            a->truncated ? " (list truncated)" : "");
    page_add(p, STYLE_DIM, "  %-4s  %-6s  %8s", "Sig", "OEM ID", "Length");
    for (size_t i = 0; i < a->table_count; i++) {
        const struct acpi_table *t = &a->tables[i];
        if (!t->mapped)
            page_add(p, STYLE_WARN, "  %-4s  %-6s  %8s  (unmapped)", t->sig, "-", "-");
        else
            page_add(p, t->checksum_ok ? STYLE_TEXT : STYLE_WARN, "  %-4s  %-6s  %8u%s",
                     t->sig, t->oem_id, t->length, t->checksum_ok ? "" : "  (bad csum)");
    }
}
