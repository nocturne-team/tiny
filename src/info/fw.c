#include "info/fw.h"
#include "boot.h"
#include "gfx/gfx.h"
#include "info/rtc.h"
#include "lib/fmt.h"
#include "lib/serial.h"
#include "lib/string.h"

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
        acpi_parse(bootinfo.rsdp, bootinfo.hhdm, &f->acpi);
        smbios_parse(bootinfo.smbios32, bootinfo.smbios64, bootinfo.hhdm, &f->smbios);
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
}
