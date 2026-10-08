#include "info/edid.h"
#include "lib/fmt.h"
#include "lib/string.h"

static const uint8_t header[8] = { 0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x00 };

static uint64_t isqrt(uint64_t v) {
    uint64_t r = 0, bit = 1ULL << 62;
    while (bit > v)
        bit >>= 2;
    while (bit) {
        if (v >= r + bit) {
            v -= r + bit;
            r = (r >> 1) + bit;
        } else {
            r >>= 1;
        }
        bit >>= 2;
    }
    return r;
}

static void detailed_timing(const uint8_t *d, struct edid_info *out) {
    uint32_t clock = (uint32_t)(d[0] | d[1] << 8) * 10000u;  // Hz
    uint32_t hactive = d[2] | (uint32_t)(d[4] & 0xF0) << 4;
    uint32_t hblank = d[3] | (uint32_t)(d[4] & 0x0F) << 8;
    uint32_t vactive = d[5] | (uint32_t)(d[7] & 0xF0) << 4;
    uint32_t vblank = d[6] | (uint32_t)(d[7] & 0x0F) << 8;
    uint64_t total = (uint64_t)(hactive + hblank) * (vactive + vblank);
    out->pref_width = (uint16_t)hactive;
    out->pref_height = (uint16_t)vactive;
    out->pref_centihz = total ? (uint32_t)(((uint64_t)clock * 100 + total / 2) / total) : 0;
}

void edid_parse(const uint8_t *e, size_t len, struct edid_info *out) {
    memset(out, 0, sizeof *out);
    if (!e || len < 128 || memcmp(e, header, 8) != 0)
        return;
    out->valid = true;

    uint8_t sum = 0;
    for (int i = 0; i < 128; i++)
        sum = (uint8_t)(sum + e[i]);
    out->checksum_ok = sum == 0;

    uint16_t id = (uint16_t)(e[8] << 8 | e[9]);
    out->manufacturer[0] = (char)('@' + ((id >> 10) & 0x1F));
    out->manufacturer[1] = (char)('@' + ((id >> 5) & 0x1F));
    out->manufacturer[2] = (char)('@' + (id & 0x1F));
    out->product = (uint16_t)(e[10] | e[11] << 8);
    out->serial = (uint32_t)e[12] | (uint32_t)e[13] << 8 | (uint32_t)e[14] << 16 | (uint32_t)e[15] << 24;
    out->model_year = e[16] == 0xFF;
    out->week = out->model_year ? 0 : e[16];
    out->year = 1990 + e[17];
    out->version = e[18];
    out->revision = e[19];
    out->width_cm = e[21];
    out->height_cm = e[22];
    if (out->width_cm && out->height_cm) {
        uint64_t w = out->width_cm * 10u, h = out->height_cm * 10u;  // mm
        out->diagonal_tenths = (unsigned)((isqrt(w * w + h * h) * 100 + 127) / 254);
    }

    // Four 18-byte descriptors: detailed timings, or display descriptors (tag in byte 3).
    for (int i = 0; i < 4; i++) {
        const uint8_t *d = e + 54 + i * 18;
        if (d[0] || d[1]) {
            if (!out->pref_width)
                detailed_timing(d, out);
        } else if (d[3] == 0xFC && !out->name[0]) {
            size_t n = 0;
            while (n < 13 && d[5 + n] != '\n')
                n++;
            copy_printable(out->name, sizeof out->name, (const char *)d + 5, n);
        }
    }
}
