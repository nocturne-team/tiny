#pragma once
#include <stdbool.h>
#include <stdint.h>
#include "info/acpi.h"
#include "info/edid.h"
#include "info/smbios.h"
#include "lib/time.h"

struct fw_info {
    int firmware_type;         // -1 unknown, else LIMINE_FIRMWARE_TYPE_*
    char bootloader[64];       // "Limine 11.4.1", or "" if unknown
    bool have_boot_time;
    struct datetime boot_time; // UTC, from Limine
    uint64_t fb_width, fb_height, fb_pitch;
    uint16_t fb_bpp;
    int ui_cols, ui_rows, ui_scale;
    bool rtc_ok, rtc_bcd, rtc_24h;
    bool have_edid;            // Limine passed EDID bytes for the display
    struct edid_info edid;
    struct acpi_info acpi;
    struct smbios_info smbios;
};

// Gathers firmware facts. Call after gfx_init().
void fw_probe(struct fw_info *out);
